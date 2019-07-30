#include <Log.h>
#include <Options.h>
#include <algorithm>
#include <arpa/inet.h>
#include <bencode.h>
#include <bootstrap.h>
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


// get_peers resp str['value': 5, hex[4E11B720F9F028A457FF5745CF3E5B48BF0917E72FBDE8E914E7EC14EA7C7ADE9EE646DC948FE9A95951BC133B2111104E7EB26E33A2695B516CC73A35EA28C9FB85D6E2EAC4EF2C2104E7B0D6AE529049F1F1BBE9EBB3A6DB3C87CE1C2BA99CA83CF4E7807D4285B498E7D147C9DDED355AFE8E58C030D1AE14E798E3F720AD993A4991767B94336696FC45B34F7E1F17BF694E5BD8B5DE5252DC84BFF34EB2C3A4C5BF792B56795C6BB8A84E4516D11E237509F7875168162D119B744CA5BC8F418FE96B](N___ __(_W_WE_>_____~r____N___________F_____YQ__;!__N__&_:&____s_^____]n._N___N____R_I________<_________N__}______}_|____Z__X_0___N____ __:I_v{_3f__E_O~___iN___]_%-_K_4_,:___y+Vy\k__N_Qm__7P_xu___-__tL___A__k)]
// 2019-07-28 19:21:25|parse error|'get_peers' response missing 'id' and 'token' or ('nodes' or 'values')|
// d
//  2:id
//  20:hex[4E7AA36D825AD96A5D1AFC8318A738457CDDDC2]: 20(N__6_%______1_s_W___)
//  5:token
//  20:hex[4E7AA36D825AD96A5D1AFC8318A738457CDDDC2]: 20(N__6_%______1_s_W___)
//  5:value
//  208:hex[4E11B720F9F028A457FF5745CF3E5B48BF0917E72FBDE8E914E7EC14EA7C7ADE9EE646DC948FE9A95951BC133B2111104E7EB26E33A2695B516CC73A35EA28C9FB85D6E2EAC4EF2C2104E7B0D6AE529049F1F1BBE9EBB3A6DB3C87CE1C2BA99CA83CF4E7807D4285B498E7D147C9DDED355AFE8E58C030D1AE14E798E3F720AD993A4991767B94336696FC45B34F7E1F17BF694E5BD8B5DE5252DC84BFF34EB2C3A4C5BF792B56795C6BB8A84E4516D11E237509F7875168162D119B744CA5BC8F418FE96B]: 208(N___ __(_W_WE_>_____~r____N___________F_____YQ__;!__N__&_:&____s_^____]n._N___N____R_I________<_________N__}______}_|____Z__X_0___N____ __:I_v{_3f__E_O~___iN___]_%-_K_4_,:___y+Vy\k__N_Qm__7P_xu___-__tL___A__k)
// e

// 2019-07-28 19:37:28|parse error|'get_peers' response missing 'id' and 'token' or ('nodes' or 'values')|
// d
//  2:id
//  20:hex[A8D63B2AFA6A67B8E850D9809196E0F3D2D073D]: 20(__;*_jg__P________s_)
//  5:token
//  8:hex[D2C566FE215FC668]: 8(__f_!__h)
// e

// TODO !implement peer db timeout logic
// TODO fix db read logic

// TODO getopt: repeating bootstrap nodes

// TODO
// - cache tx raw sent and print when parse error response to file
// - find_response & others should be able to handle error response
// TODO BytesView implement mark
// TODO log explicit error response (error module)
// XXX ipv6
// TODO client: multiple receiver for the same search
// TODO client: on server shutdown send to search clients that we are shutting
// down
// TODO replace bad node
// TODO db file
//      - incremental save
//      - save to multiple db files
//      - load from multiple db files
//      - naming scheme
// TODO interrupt
//    - SIGHUP ^C QUIT, ...
//      - save db file and quit
//
// TODO if both eth0 & wlan0 there is some problem
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
    if (!to_contact(ip, bs)) {
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
  if (!dht_upnp::setup(modules)) {
    die("dht_upnp::setup(modules)");
  }

  auto handle_cb = [&](Contact from, sp::Buffer &in, sp::Buffer &out,
                       Timestamp now) {
    mdht->last_activity =
        mdht->last_activity == Timestamp(0) ? now : mdht->last_activity;
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
