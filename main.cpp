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

static std::unique_ptr<dht::DHT> mdht;
const char *dump_file = "/tmp/dht_db.dump2";

// static void
// sighandler(int) {
//   // printf("Caught signal %d, coming out...\n", signum);
//   if (mdht) {
//     // if (!sp::dump(*mdht, dump_file)) {
//     //   // printf("failed dump\n");
//     // }
//   } else {
//     // printf("failed to dump: dht is nullptr\n");
//   }
//
//   exit(1);
// }

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
static void
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

          on_int(info);
          return;
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
      if (tx::take(dht.client, pctx.tx, context)) {
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
  if (!krpc::d::krpc(pctx, f)) {
    return false;
  }
  return true;
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
    return 0;
  }
  std::srand((unsigned int)time(nullptr));

  fd udp = udp::bind(options.port, udp::Mode::NONBLOCKING);
  // fd udp = udp::bind(INADDR_ANY, 0);

  prng::xorshift32 r(14);

  Contact self;
  if (!convert("81.232.82.13:0", self)) {
    die("TODO");
  }

  mdht = std::make_unique<dht::DHT>(udp, self, r);
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
      "195.191.186.170:30337",
      // }
      //
      // "0.0.0.0:51413",      //
      // "192.168.2.14:51413",
      // "127.0.0.1:51413", "213.65.130.80:51413",
  };
  mdht->now = sp::now();
  for_each(bss, [](const char *ip) {
    Contact bs(0, 0);
    if (!convert(ip, bs)) {
      die("parse bootstrap ip failed");
    }

    assert(bs.ip.ipv4 > 0);
    assert(bs.port > 0);

    Contact node(bs);

    if (sp::insert(mdht->bootstrap_contacts, node) == nullptr) {
      die("failed to setup bootstrap");
    }
  });
  fd sfd = setup_signal();

  fd poll = setup_epoll(udp, sfd);

  dht::Modules modules;
  {
    if (!interface_priv::setup(modules)) {
      die("interface_priv::setup(modules)");
    }
    if (!interface_dht::setup(modules)) {
      die("interface_dht::setup(modules)");
    }
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

  auto awake_cb = [&modules](sp::Buffer &out, Timestamp now) {
    print_result(mdht->election);
    mdht->now = now;

    dht::Config config;
    Timeout result = config.refresh_interval;
    result = reduce(modules.on_awake, result, [&out](auto acum, auto callback) {
      auto cr = callback(*mdht, out);
      return std::min(cr, acum);
    });

    log::awake::timeout(*mdht, result);
    mdht->last_activity = now;
    return result;
  };

  auto interrupt_cb = [](const signalfd_siginfo &info) {
    printf("signal: %s\n", strsignal(info.ssi_signo));
    // TODO only if is warmed up so we are not 
    // sp::dump(dht,);

    /**/
    return;
  };

  loop(poll, sfd, handle_cb, awake_cb, interrupt_cb);
  return 0;
}
