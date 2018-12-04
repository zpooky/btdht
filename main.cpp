#include "dht.h"
#include "udp.h"
#include <cstdio>

#include "dht_interface.h"
#include "dump.h"
#include "private_interface.h"

#include "Log.h"
#include "Options.h"
#include "bencode.h"
#include "krpc.h"
#include "shared.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <exception>
#include <memory>
#include <signal.h>
#include <sys/epoll.h>    //epoll
#include <sys/signalfd.h> //signalfd
#include <unistd.h>       //read

// TODO getopt: repeating bootstrap nodes

// TODO
// dht.c:
// MinHeap of buckets
// unallocated is of prio:0
// each level of allocated buckets has id of level first id: 1
// - if a node does not fit in a bucket and can not replace a existing and the
//   bucket can not be split into two buckets:
//   1. try take from top of MinHeap if MinHeap.peek.id != current_bucket.id
//   2. if already allocated bucket: then do a controlled remove of member
//      contacts
//   3. stick new bucket as a linked list onto current_bucket
//   4. insert new allocated bucket into MinHeap with id of current level
//
// ...
// - MinHeap should be sized based on seek_precent_config?
// - There should be a prefix length which represent the number of buckets
//   which does not have a bucket since it has been recycled and used to hold
//   more precise contacts. the prefix length is used to denote the start of
//   self.id...

static const char *const dump_file = "/tmp/dht_db.dump2";

static void
die(const char *s) {
  perror(s);
  std::exit(1);
}

static fd
setup_signal() {
  /* Fetch current signal mask */
  sigset_t sigset;
  if (sigprocmask(SIG_SETMASK, NULL, &sigset) < 0) {
    die("get sigprocmask");
  }

  /* Block signals so that they aren't handled
   * according to their default dispositions
   */
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGHUP);

  /* Modify signal mask */
  if (sigprocmask(SIG_SETMASK, &sigset, NULL) < 0) {
    die("modify sigprocmask");
  }

  /* Setup signal to read */
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGHUP);

  int flags = SFD_NONBLOCK | SFD_CLOEXEC;
  int sfd = signalfd(/*new fd*/ -1, &sigset, flags);
  if (sfd < 0) {
    die("signalfd");
  }

  return fd{sfd};
}

static fd
setup_epoll(fd &udp, fd &signal) noexcept {
  const int flag = 0;
  const int pfd = ::epoll_create1(flag);
  if (pfd < 0) {
    die("epoll_create1");
  }

  ::epoll_event ev;
  // this are the events we are polling for
  // `man epoll_ctl` for list of events
  // EPOLLIN: ready for read()
  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
  ev.data.fd = int(udp);

  // add fd to poll on
  if (::epoll_ctl(pfd, EPOLL_CTL_ADD, int(udp), &ev) < 0) {
    die("epoll_ctl: listen_udp");
  }

  ev.events = EPOLLIN;
  ev.data.fd = int(signal);
  if (::epoll_ctl(pfd, EPOLL_CTL_ADD, int(signal), &ev) < 0) {
    die("epoll_ctl: listen_signal");
  }

  return fd{pfd};
}

template <typename Handle, typename Awake, typename Interrupt>
static int
loop(fd &pfd, fd &sfd, Handle handle, Awake on_awake,
     Interrupt on_int) noexcept {
  Timestamp previous(0);

  constexpr std::size_t size = 10 * 1024 * 1024;
  auto in = new sp::byte[size];
  auto out = new sp::byte[size];

  sp::Milliseconds timeout(0);
  for (;;) {

    constexpr std::size_t max_events = 64;
    ::epoll_event events[max_events];

    int no_events = ::epoll_wait(int(pfd), events, max_events, int(timeout));
    if (no_events < 0) {
      if (errno == EAGAIN) {
      } else {
        die("epoll_wait");
      }
    }

    // always increasing clock
    Timestamp now = std::max(sp::now(), previous);

    for (int i = 0; i < no_events; ++i) {
      ::epoll_event &current = events[i];

      if (current.events & EPOLLIN) {
        int cfd = current.data.fd;
        if (cfd == int(sfd)) {
          signalfd_siginfo info;

          constexpr std::size_t len = sizeof(info);
          if (::read(int(sfd), (void *)&info, len) != len) {
            die("read(signal)");
          }

          return on_int(info);
        } else {
          sp::Buffer inBuffer(in, size);
          sp::Buffer outBuffer(out, size);

          Contact from;
          udp::receive(cfd, /*OUT*/ from, inBuffer);
          flip(inBuffer);

          if (inBuffer.length > 0) {
            handle(from, inBuffer, outBuffer, now);
            flip(outBuffer);

            if (outBuffer.length > 0) {
              udp::send(cfd, /*to*/ from, outBuffer);
            }
          }
        }
      }

      if (current.events & EPOLLERR) {
        printf("EPOLLERR\n");
      }
      if (current.events & EPOLLHUP) {
        printf("EPOLLHUP\n");
      }
      if (current.events & EPOLLOUT) {
        printf("EPOLLOUT\n");
      }
    } // for

    sp::Buffer outBuffer(out, size);
    timeout = on_awake(outBuffer, now);

    previous = now;
  } // for

  return 0;
}

static bool
parse(dht::DHT &dht, dht::Modules &modules, const Contact &peer, sp::Buffer &in,
      sp::Buffer &out) noexcept {

  auto f = [&dht, &modules, &peer, &out](krpc::ParseContext &pctx) {
    dht::Module error;
    error::setup(error);

    dht::MessageContext ctx{dht, pctx, out, peer};
    if (std::strcmp(pctx.msg_type, "q") == 0) {
      /*query*/
      if (!bencode::d::value(pctx.decoder, "a")) {
        return false;
      }

      dht::Module &m = module_for(modules, pctx.query, error);

      return m.request(ctx);
    } else if (std::strcmp(pctx.msg_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(pctx.decoder, "r")) {
        return false;
      }

      tx::TxContext context;
      std::size_t cnt = dht.client.active;
      if (tx::consume(dht.client, pctx.tx, context)) {
        assertx((cnt - 1) == dht.client.active);
        log::receive::res::known_tx(ctx);
        bool res = context.handle(ctx);
        return res;
      } else {
        log::receive::res::unknown_tx(ctx);
        // assertx(false);
      }
    }

    return false;
  };

  krpc::ParseContext pctx(in);
  return krpc::d::krpc(pctx, f);
}

template <typename T, std::size_t SIZE, typename F>
static void
for_each(T (&arr)[SIZE], F f) noexcept {
  for (std::size_t i = 0; i < SIZE; ++i) {
    f(arr[i]);
  }
}

// transmission-daemon -er--dht
// echo "asd" | netcat --udp 127.0.0.1 45058
int
main(int argc, char **argv) {
  printf("sizeof(DHT): %zuB %zuKB\n", sizeof(dht::DHT),
         sizeof(dht::DHT) / 1024);
  dht::Options options;
  if (!dht::parse(argc, argv, options)) {
    //   die("TODO");
    return 1;
  }
  std::srand((unsigned int)time(nullptr));

  fd sfd = setup_signal();
  fd udp = udp::bind(options.port, udp::Mode::NONBLOCKING);
  // fd udp = udp::bind(INADDR_ANY, 0);

  prng::xorshift32 r(14);

  Contact self = udp::local(udp);
  auto mdht = std::make_unique<dht::DHT>(udp, self, r);
  if (!dht::init(*mdht)) {
    die("failed to init dht");
  }

  printf("node id: ");
  dht::print_hex(mdht->id);

  if (!sp::restore(*mdht, dump_file)) {
    die("restore failed\n");
  }

  {
    Contact local = udp::local(udp);
    char str[256] = {0};
    assertx(to_string(local, str, sizeof(str)));
    printf("bind(%s)\n", str);
  }

  mdht->now = sp::now();

  /*boostrap*/
  // Contact bs_node(INADDR_ANY, local.port); // TODO
  const char *bss[] = {
      // "192.168.1.47:13596",
      // "127.0.0.1:13596",
      // "213.65.130.80:13596",
      // start {
      "192.168.1.49:51413",   //
      "192.168.2.14:51413",   //
      "109.228.170.47:51413", //

      "192.168.0.15:13596",  //
      "127.0.0.1:13596",     //
      "192.168.0.113:13596", //
      "192.168.1.49:13596",  //
      //
      "213.174.1.219:13680",  //
      "90.243.184.9:6881",    //
      "24.34.3.237:6881",     //
      "105.99.128.147:40189", //
      "79.160.16.63:6881",    //
      "213.174.1.219:13680",  //
      "24.34.3.237:6881",     //
      "2.154.168.153:6881",   //
      "47.198.79.139:50321",  //
      "94.181.155.240:47661", //
      "47.198.79.139:50321",  //
      "213.93.18.70:24896",   //
      "173.92.231.220:6881",  //
      "93.80.248.65:8999",    //
      "95.219.168.133:50321", //
      "85.247.221.231:19743", //
      "62.14.189.18:57539",   //
      //
      "195.191.186.170:30337",
      // }
      //
      // "0.0.0.0:51413",      //
      // "192.168.2.14:51413",
      // "127.0.0.1:51413", "213.65.130.80:51413",
  };

  for_each(bss, [&mdht](const char *ip) {
    Contact bs(0, 0);
    if (!convert(ip, bs)) {
      die("parse bootstrap ip failed");
    }

    assertx(bs.ip.ipv4 > 0);
    assertx(bs.port > 0);

    // TODO bootstrap should be a circular linked list and include a sent date
    // and a outstanding request counter for each entry
    insert_unique(mdht->bootstrap_contacts, bs);
  });

  fd poll = setup_epoll(udp, sfd);

  dht::Modules modules;
  if (!interface_priv::setup(modules)) {
    die("interface_priv::setup(modules)");
  }
  if (!interface_dht::setup(modules)) {
    die("interface_dht::setup(modules)");
  }

  auto handle_cb = [&](Contact from, sp::Buffer &in, sp::Buffer &out,
                       Timestamp now) {
    mdht->last_activity =
        mdht->last_activity == Timestamp(0) ? now : mdht->last_activity;
    mdht->now = now;

    const sp::Buffer in_view(in);
    if (!parse(*mdht, modules, from, in, out)) {
      log::receive::parse::error(*mdht, in_view);
      return false;
    }

    return true;
  };

  auto awake_cb = [&mdht, &modules](sp::Buffer &out, Timestamp now) {
    printf("\n");
    print_result(mdht->election);
    mdht->now = now;

    Timeout result = mdht->config.refresh_interval;
    result = reduce(modules.on_awake, result,
                    [&mdht, &out](auto acum, auto callback) {
                      Timeout cr = callback(*mdht, out);
                      return std::min(cr, acum);
                    });

    log::awake::timeout(*mdht, result);
    mdht->last_activity = now;

    return result;
  };

  auto interrupt_cb = [&mdht](const signalfd_siginfo &info) {
    printf("signal: %s\n", strsignal(info.ssi_signo));

    // TODO handle more signals
    if (mdht) {
      // TODO only if is warmed up so we are not
      if (!sp::dump(*mdht, dump_file)) {
        return 2;
      }
    }

    /**/
    return 1;
  };

  return loop(poll, sfd, handle_cb, awake_cb, interrupt_cb);
}
