#include <Log.h>
#include <Options.h>
#include <algorithm>
#include <arpa/inet.h>
#include <bencode.h>
#include <cache.h>
#include <cstdio>
#include <cstring>
#include <dht.h>
#include <dht_interface.h>
#include <dump.h>
#include <errno.h>
#include <exception>
#include <krpc.h>
#include <memory>
#include <private_interface.h>
#include <shared.h>
#include <signal.h>
#include <sys/epoll.h>    //epoll
#include <sys/signalfd.h> //signalfd
#include <udp.h>
#include <unistd.h> //read
#include <upnp_service.h>

// TODO !!better upnp 
// TODO !implement peer db timeout logic
// TODO fix db read logic

// TODO getopt: repeating bootstrap nodes

// TODO
// - cache tx raw sent and print when parse error response to file
// - find_response & others should be able to handle error response
// TODO BytesView implement mark
// TODO log explicit error response (error module)
// XXX ipv6
// XXX client: multiple receiver for the same search
// TODO client: on server shutdown send to search clients that we are shutting
// down
// TODO replace bad node
// TODO if both eth0 & wlan0 is active there is some problem
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
  // ^C
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGTERM);
  // sigaddset(&sigset, SIGHUP);

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
      if (errno == EAGAIN || errno == EINTR) {
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

  delete[] in;
  delete[] out;

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

      dht::Module &m = module_for(modules, pctx.query, /*default*/ unknown);

      return m.request(mctx);
    } else if (std::strcmp(pctx.msg_type, "r") == 0) {
      /*response*/

      tx::TxContext tctx;
      std::size_t cnt = dht.client.active;
      if (tx::consume(dht.client, pctx.tx, tctx)) {
        assertx((cnt - 1) == dht.client.active);
        log::receive::res::known_tx(mctx);
        return tctx.handle(mctx);
      } else {
        log::receive::res::unknown_tx(mctx, in);
        // assertx(false);
      }
    } else if (std::strcmp(pctx.msg_type, "e") == 0) {
      // TODO
    } else {
      assertx(false);
    }

    return false;
  };

  krpc::ParseContext pctx(dht, in);
  return krpc::d::krpc(pctx, handle);
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
    return 2;
  }

  fd udp = udp::bind_v4(options.port, udp::Mode::NONBLOCKING);
  if (!udp) {
    printf("failed to bind: %u\n", options.port);
    return 3;
  }

  Contact listen;
  if (!udp::local(udp, listen)) {
    return 3;
  }

  auto r = prng::seed<prng::xorshift32>();
  auto mdht = std::make_unique<dht::DHT>(udp, listen, r);
  if (!dht::init(*mdht)) {
    die("failed to init dht");
    return 4;
  }

  if (!sp::init_cache(*mdht)) {
    die("failed to init cache");
    return 5;
  }

  if (!sp::restore(*mdht, options.dump_file)) {
    die("restore failed\n");
  }

  printf("node id: %s\n", to_hex(mdht->id));

  {
    char str[256] = {0};
    assertx(to_string(mdht->ip, str, sizeof(str)));
    printf("remote(%s)\n", str);
    assertx(to_string(listen, str, sizeof(str)));
    printf("bind(%s)\n", str);
  }

  mdht->now = sp::now();

  fd poll = setup_epoll(udp, sfd);
  if (!poll) {
    return 6;
  }

  dht::Modules modules;
  if (!interface_priv::setup(modules)) {
    die("interface_priv::setup(modules)");
  }
  if (!interface_dht::setup(modules)) {
    die("interface_dht::setup(modules)");
  }
  if (!dht_upnp::setup(modules)) {
    die("dht_upnp::setup(modules)");
  }

  auto handle_cb = [&](Contact from, sp::Buffer &in, sp::Buffer &out,
                       Timestamp now) {
    if (mdht->last_activity == Timestamp(0)) {
      mdht->last_activity = now;
    }
    mdht->now = now;

    const sp::Buffer in_view(in);
    if (!parse(*mdht, modules, from, in, out)) {
      return false;
    }

    return true;
  };

  auto awake_cb = [&mdht, &modules](sp::Buffer &out, Timestamp now) {
    // print_result(mdht->election);
    mdht->now = now;

    Timeout result = mdht->config.refresh_interval;
    auto cb = [&mdht, &out](auto acum, auto callback) {
      Timeout cr = callback(*mdht, out);
      return std::min(cr, acum);
    };
    result = reduce(modules.on_awake, result, cb);

    log::awake::timeout(*mdht, result);
    mdht->last_activity = now;

    return result;
  };

  auto interrupt_cb = [&](const signalfd_siginfo &info) {
    printf("signal: %s: %d\n", strsignal(info.ssi_signo), info.ssi_signo);

    if (mdht) {
      sp::deinit_cache(*mdht);
      if (!sp::dump(*mdht, options.dump_file)) {
        return 2;
      }
    }

    /**/
    return 1;
  };

  return main_loop(poll, sfd, handle_cb, awake_cb, interrupt_cb);
}
