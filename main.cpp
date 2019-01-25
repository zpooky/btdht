#include "dht.h"
#include "udp.h"
#include <cstdio>

#include "bootstrap.h"
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
// - cache tx raw sent and print when parse error response to file
// - find_response & others should be able to handle error response
// TODO BytesView implement mark
// TODO log explicit error response (error module)
// TODO actively timeout transactions so that they return back into $bootstrap
// heap so that we do not starve for and dead lock ourself. Since now we only
// time out transaction lazily when we issue new requests and if there is no
// nodes to send request to we do not timeout existing transaction and never
// gets back bootstrap nodes and we starve forever.
// TODO enumrate interface bound to (0.0.0.0) -> find default gateways -> send
// upnp port mapping
// TODO implement peer db timeout logic
// XXX ipv6
// TODO cleint: multiple receiver for the same search
// TODO client: debug search result message is valid/bencode_print list works
// correcly
// TODO bencode_print hex id does not works? only print len(39) where len(40) is
// epexcted.
// TODO client: on server shutdown send to search clients that we are shuting
// down
// TODO replace bad node
static void
die(const char *s) {
  perror(s);
  std::exit(1);
}

static fd
setup_signal() {
  // return fd{0};
  /* Fetch current signal mask */
  sigset_t sigset;
  if (sigprocmask(SIG_SETMASK, NULL, &sigset) < 0) {
    die("get sigprocmask");
  }

  /* Block signals so that they aren't handled
   * according to their default dispositions
   */
#if 0
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGHUP);
#endif
  sigfillset(&sigset);

  /*job control*/
  sigdelset(&sigset, SIGCONT);
  sigdelset(&sigset, SIGTSTP);

  /* Modify signal mask */
  if (sigprocmask(SIG_SETMASK, &sigset, NULL) < 0) {
    die("modify sigprocmask");
  }

  /* Setup signal to read */
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGHUP);

  // sigfillset(&sigset);
  // sigdelset(&sigset, SIGWINCH);

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
main_loop(fd &pfd, fd &sfd, Handle handle, Awake on_awake,
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

    printf("============================\n");
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
          int res = 0;
          while (res == 0) {
            sp::Buffer inBuffer(in, size);
            sp::Buffer outBuffer(out, size);

            Contact from;
            res = udp::receive(cfd, /*OUT*/ from, inBuffer);
            if (res == 0) {
              flip(inBuffer);

              if (inBuffer.length > 0) {
                handle(from, inBuffer, outBuffer, now);
                flip(outBuffer);

                if (outBuffer.length > 0) {
                  udp::send(cfd, /*to*/ from, outBuffer);
                }
              }
            }
          } // while
        }   // else
      }     // if EPOLLIN

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

  auto handle = [&](krpc::ParseContext &pctx) {
    dht::Module unknown;
    error::setup(unknown);

    dht::MessageContext mctx{dht, pctx, out, peer};
    if (std::strcmp(pctx.msg_type, "q") == 0) {
      /*query*/
      if (!bencode::d::value(pctx.decoder, "a")) {
        return false;
      }

      dht::Module &m = module_for(modules, pctx.query, /*default*/ unknown);

      return m.request(mctx);
    } else if (std::strcmp(pctx.msg_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(pctx.decoder, "r")) {
        return false;
      }

      tx::TxContext tctx;
      std::size_t cnt = dht.client.active;
      if (tx::consume(dht.client, pctx.tx, tctx)) {
        assertx((cnt - 1) == dht.client.active);
        log::receive::res::known_tx(mctx);
        bool res = tctx.handle(mctx);
        return res;
      } else {
        log::receive::res::unknown_tx(mctx);
        // assertx(false);
      }
    }

    return false;
  };

  krpc::ParseContext pctx(in);
  return krpc::d::krpc(pctx, handle);
}

template <typename T, std::size_t SIZE, typename F>
static void
for_each(T (&arr)[SIZE], F f) noexcept {
  for (std::size_t i = 0; i < SIZE; ++i) {
    f(arr[i]);
  }
}

bool
setup_bootstrap(dht::DHT &self) noexcept {
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

  for_each(bss, [&self](const char *ip) {
    Contact bs;
    if (!convert(ip, bs)) {
      die("parse bootstrap ip failed");
    }

    assertx(bs.ip.ipv4 > 0);
    assertx(bs.port > 0);

    bootstrap_insert(self, dht::KContact(0, bs));
  });

  printf("total bootstrap(%zu)\n", length(self.bootstrap));
  return true;
}

// transmission-daemon -er--dht
// echo "asd" | netcat --udp 127.0.0.1 45058
int
main(int argc, char **argv) {
  printf("sizeof(DHT): %zuB %zuKB\n", sizeof(dht::DHT),
         sizeof(dht::DHT) / 1024);
  dht::Options options;
  if (!dht::parse(options, argc, argv)) {
    return 1;
  }
  std::srand((unsigned int)time(nullptr));

  fd sfd = setup_signal();
  if (!sfd) {
    return 1;
  }

  fd udp = udp::bind_v4(options.port, udp::Mode::NONBLOCKING);
  if (!udp) {
    return 1;
  }

  Contact listen;
  if (!udp::local(udp, listen)) {
    return 2;
  }

  auto r = prng::seed<prng::xorshift32>();
  auto mdht = std::make_unique<dht::DHT>(udp, listen, r);
  if (!dht::init(*mdht)) {
    die("failed to init dht");
    return 3;
  }

  if (!sp::restore(*mdht, options.dump_file)) {
    die("restore failed\n");
  }

  printf("node id: %s\n", to_hex(mdht->id));

  printf("bootstrap from db(%zu)\n", length(mdht->bootstrap));
  {
    char str[256] = {0};
    assertx(to_string(mdht->ip, str, sizeof(str)));
    printf("remote(%s)\n", str);
    assertx(to_string(listen, str, sizeof(str)));
    printf("bind(%s)\n", str);
  }

  mdht->now = sp::now();

  if (!setup_bootstrap(*mdht)) {
    return 1;
  }

  fd poll = setup_epoll(udp, sfd);
  if (!poll) {
    return 4;
  }

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
    // print_result(mdht->election);
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

  auto interrupt_cb = [&](const signalfd_siginfo &info) {
    printf("signal: %s: %d\n", strsignal(info.ssi_signo), info.ssi_signo);

    if (mdht) {
      // TODO only if is warmed up so we are not
      if (!sp::dump(*mdht, options.dump_file)) {
        return 2;
      }
    }

    /**/
    return 1;
  };

  return main_loop(poll, sfd, handle_cb, awake_cb, interrupt_cb);
}
