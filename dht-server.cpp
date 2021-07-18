#include <Log.h>
#include <Options.h>
#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <dht.h>
#include <dump.h>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <io/file.h>
#include <krpc.h>
#include <memory>
#include <shared.h>
#include <signal.h>
#include <sys/epoll.h>    //epoll
#include <sys/signalfd.h> //signalfd
#include <udp.h>
#include <unistd.h> //read

#include <bencode.h>
#include <cache.h>
#include <core.h>
#include <dht_interface.h>
#include <private_interface.h>
#include <upnp_service.h>

// TODO awake next timeout[0ms]
// TODO use ip not ip:port in bloomfilters
// TODO !!better upnp
// TODO if both eth0 & wlan0 is active there is some problem

// TODO getopt: repeating bootstrap nodes

// TODO
// - cache tx raw sent and print when parse error response to file
// - find_response & others should be able to handle error response
// TODO BytesView implement mark
// TODO log explicit error response (error module)
// XXX ipv6
// XXX client: multiple receiver for the same search
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
  // ^C
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGTERM);
  // sigaddset(&sigset, SIGHUP);

  // sigfillset(&sigset);
  // sigdelset(&sigset, SIGWINCH);

  int flags = SFD_NONBLOCK | SFD_CLOEXEC;
  int signal_fd = signalfd(/*new fd*/ -1, &sigset, flags);
  if (signal_fd < 0) {
    die("signalfd");
  }

  return fd{signal_fd};
}

static int
on_dht_protocol_handle(void *callback, uint32_t events);

struct dht_protocol_callback {
  sp::core_callback core_cb;
  dht::Modules modules;
  dht::DHT &dht;
  dht::Options &options;
  fd &udp_fd;
  static constexpr std::size_t size = 16 * 1024;
  std::unique_ptr<sp::byte[]> in;
  std::unique_ptr<sp::byte[]> out;

  dht_protocol_callback(dht::ModulesAwake &awake, dht::DHT &_dht,
                        dht::Options &_options, fd &_fd)
      : core_cb{}
      , modules{awake}
      , dht{_dht}
      , options{_options}
      , udp_fd{_fd}
      , in{std::make_unique<sp::byte[]>(size)}
      , out{std::make_unique<sp::byte[]>(size)} {

    if (!interface_dht::setup(modules)) {
      die("interface_dht::setup(modules)");
    }
    core_cb.closure = this;
    core_cb.callback = on_dht_protocol_handle;
  }
};

static bool
parse(dht::DHT &dht, dht::Modules &modules, const Contact &peer, sp::Buffer &in,
      sp::Buffer &out) noexcept {
  auto handle = [&](krpc::ParseContext &pctx) {
    dht::Module unknown;
    error::setup(unknown);

    dht::MessageContext mctx{dht, pctx, out, peer};
    if (std::strcmp(pctx.msg_type, "q") == 0) { /*query*/
      dht::Module &m = module_for(modules, pctx.query, /*default*/ unknown);
      return m.request(mctx);
    } else if (std::strcmp(pctx.msg_type, "r") == 0) { /*response*/
      tx::TxContext tctx;
      std::size_t cnt = dht.client.active;
      if (tx::consume(dht.client, pctx.tx, tctx)) {
        assertx((cnt - 1) == dht.client.active);
        logger::receive::res::known_tx(mctx);
        return tctx.handle(mctx);
      } else {
        logger::receive::res::unknown_tx(mctx, in);
        // assertx(false);
      }
    } else if (std::strcmp(pctx.msg_type, "e") == 0) { /*error*/
      tx::TxContext tctx;
      if (tx::consume(dht.client, pctx.tx, tctx)) {
        logger::receive::res::known_tx(mctx);
      } else {
        logger::receive::res::unknown_tx(mctx, in);
      }
      bencode_print_out(stderr);
      sp::Buffer copy(in);
      copy.pos = 0;
      bencode_print(copy);
      // TODO
      // assertx(false);
    } else {
      assertx(false);
    }

    return false;
  };

  krpc::ParseContext pctx(dht, in);
  return krpc::d::krpc(pctx, handle);
}

static int
on_dht_protocol_handle(void *callback, uint32_t events) {
  auto self = (dht_protocol_callback *)callback;
  int res = 0;

  while (res == 0) {
    sp::Buffer inBuffer(self->in.get(), self->size);
    sp::Buffer outBuffer(self->out.get(), self->size);

    Contact from;
    res = udp::receive(self->udp_fd, /*OUT*/ from, inBuffer);
    if (res == 0) {
      flip(inBuffer);

      if (inBuffer.length > 0) {
        assertx(from.port != 0);
        if (self->dht.last_activity == Timestamp(0)) {
          self->dht.last_activity = self->dht.now;
        }

        assertx(from.port != 0);
        const sp::Buffer in_view(inBuffer);
        if (!parse(self->dht, self->modules, from, inBuffer, outBuffer)) {
          return 0;
        }
        flip(outBuffer);

        if (outBuffer.length > 0) {
          udp::send(self->udp_fd, /*to*/ from, outBuffer);
        }
      }
    }
  } // while
  return 0;
}

static int
on_priv_protocol_accept_callback(void *closure, uint32_t events);

struct priv_protocol_accept_callback {
  sp::core_callback core_cb;
  dht::Modules modules;
  dht::DHT &dht;
  dht::Options &options;
  fd &priv_fd;

  priv_protocol_accept_callback(dht::ModulesAwake &awake, dht::DHT &_dht,
                                dht::Options &_options, fd &_fd)
      : core_cb{}
      , modules{awake}
      , dht{_dht}
      , options{_options}
      , priv_fd{_fd} {
    if (!interface_priv::setup(modules)) {
      die("interface_priv::setup(modules)");
    }
    core_cb.closure = this;
    core_cb.callback = on_priv_protocol_accept_callback;
  }
};

static int
on_priv_protocol_callback(void *closure, uint32_t events);
struct priv_protocol_callback {
  sp::core_callback core_cb;
  dht::Modules &modules;
  dht::DHT &dht;
  dht::Options &options;
  fd client_fd;
  static constexpr std::size_t size = 16 * 1024;
  // TODO share these buffers better
  std::unique_ptr<sp::byte[]> in;
  std::unique_ptr<sp::byte[]> out;

  priv_protocol_callback(dht::Modules &_modules, dht::DHT &_dht,
                         dht::Options &_options, fd &&_fd)
      : core_cb{}
      , modules{_modules}
      , dht{_dht}
      , options{_options}
      , client_fd{std::move(_fd)}
      , in{std::make_unique<sp::byte[]>(size)}
      , out{std::make_unique<sp::byte[]>(size)} {

    core_cb.closure = this;
    core_cb.callback = on_priv_protocol_callback;
  }
};

static int
on_priv_protocol_accept_callback(void *closure, uint32_t events) {
  auto self = (priv_protocol_accept_callback *)closure;
  int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
  fd client_fd{::accept4(int(self->priv_fd), NULL, NULL, flags)};
  auto client = new priv_protocol_callback{self->modules, self->dht,
                                           self->options, std::move(client_fd)};
  int epoll_fd = self->dht.core.epoll_fd;
  if (!bool(client)) {
    die("accept");
  }

  ::epoll_event ev{};
  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
  ev.data.ptr = &client->core_cb;
  if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, int(client->client_fd), &ev) < 0) {
    die("epoll_ctl: accept private local");
  }
  return 0;
}

static int
on_priv_protocol_callback(void *closure, uint32_t events) {
  auto self = (priv_protocol_callback *)closure;

  if (events & EPOLLIN) {
    int res = 0;

    while (res == 0) {
      sp::Buffer inBuffer(self->in.get(), self->size);
      sp::Buffer outBuffer(self->out.get(), self->size);
      Contact from;
      if (!net::remote(self->client_fd, from)) {
        // assertx(false);
      }

      res = net::sock_read(self->client_fd, inBuffer);
      if (res == 0) {
        flip(inBuffer);

        if (inBuffer.length > 0) {
          const sp::Buffer in_view(inBuffer);
          if (!parse(self->dht, self->modules, from, inBuffer, outBuffer)) {

            return 0;
          }
          flip(outBuffer);

          if (outBuffer.length > 0) {
            net::sock_write(self->client_fd, outBuffer);
          }
        }
      }
    } // while
  } else if (events & EPOLLERR || events & EPOLLHUP || events & EPOLLRDHUP) {
    delete self;
  }
  return 0;
}

static int
on_interrupt(void *closure, uint32_t events);

struct interrupt_callback {
  sp::core_callback core_cb;
  dht::DHT &dht;
  dht::Options &options;
  fd &signal_fd;
  interrupt_callback(dht::DHT &_dht, dht::Options &_options, fd &_fd)
      : core_cb{}
      , dht{_dht}
      , options{_options}
      , signal_fd{_fd} {
    core_cb.closure = this;
    core_cb.callback = on_interrupt;
  }
};

static int
on_interrupt(void *closure, uint32_t events) {
  auto self = (interrupt_callback *)closure;
  signalfd_siginfo info{};

  constexpr std::size_t len = sizeof(info);
  if (::read(int(self->signal_fd), (void *)&info, len) != len) {
    die("read(signal)");
  }

  fprintf(stderr, "signal: %s: %d\n", strsignal((int)info.ssi_signo),
          info.ssi_signo);
  sp::deinit_cache(self->dht);
  sp::dump(self->dht, self->options.dump_file);
  self->dht.should_exit = true;
  return 0;
}

void
setup_epoll(dht::DHT &self, dht::ModulesAwake &awake, dht::Options &options,
            fd &udp_fd, fd &signal_fd, fd &priv_fd) noexcept {
  auto dp_cb = new dht_protocol_callback{awake, self, options, udp_fd};
  auto pp_cb = new priv_protocol_accept_callback{awake, self, options, priv_fd};
  auto i_cb = new interrupt_callback{self, options, signal_fd};
  ::epoll_event ev{};

  // this are the events we are polling for `man epoll_ctl` for list of events
  // EPOLLIN: ready for read()
  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
  ev.data.ptr = &dp_cb->core_cb;
  if (::epoll_ctl(self.core.epoll_fd, EPOLL_CTL_ADD, int(udp_fd), &ev) < 0) {
    die("epoll_ctl: listen_udp");
  }

  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
  ev.data.ptr = &pp_cb->core_cb;
  if (::epoll_ctl(self.core.epoll_fd, EPOLL_CTL_ADD, int(priv_fd), &ev) < 0) {
    die("epoll_ctl: listen private local");
  }

  ev.events = EPOLLIN;
  ev.data.ptr = &i_cb->core_cb;
  if (::epoll_ctl(self.core.epoll_fd, EPOLL_CTL_ADD, int(signal_fd), &ev) < 0) {
    die("epoll_ctl: listen_signal");
  }
}

template <typename Awake>
static int
main_loop(dht::DHT &self, Awake on_awake) noexcept {
  Timestamp previous(0);

  constexpr std::size_t size = 16 * 1024;
  auto out = std::make_unique<sp::byte[]>(size);

  sp::Milliseconds timeout(0);
  while (!self.should_exit) {
    // always increasing clock
    self.now = std::max(sp::now(), previous);

    core_tick(self.core, timeout);
    sp::Buffer outBuffer(out.get(), size);
    timeout = on_awake(outBuffer);

    previous = self.now;
  } // for

  return 0;
}

// transmission-daemon -er--dht
// echo "asd" | netcat --udp 127.0.0.1 45058
int
main(int argc, char **argv) {
  fprintf(stderr, "sizeof(DHT): %zuB %zuKB\n", sizeof(dht::DHT),
          sizeof(dht::DHT) / 1024);
  dht::Options options;
  if (!dht::parse(options, argc, argv)) {
    return 1;
  }
  std::srand((unsigned int)time(nullptr));

  fd signal_fd = setup_signal();
  if (!signal_fd) {
    return 2;
  }

  fd udp_fd = udp::bind_v4(options.port, udp::Mode::NONBLOCKING);
  if (!udp_fd) {
    fprintf(stderr, "failed to bind: %u\n", options.port);
    return 3;
  }

  if (strlen(options.local_socket) == 0) {
    if (!xdg_runtime_dir(options.local_socket)) {
      return 3;
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;
    if (!fs::mkdirs(options.local_socket, mode)) {
      return 3;
    }
    ::strcat(options.local_socket, "/spdht.socket");
  }
  unlink(options.local_socket);

  fd priv_fd = udp::bind_unix_seq(options.local_socket, udp::Mode::NONBLOCKING);
  if (!priv_fd) {
    fprintf(stderr, "failed to bind: local %s\n", options.local_socket);
    return 3;
  }

  Contact listen;
  if (!net::local(udp_fd, listen)) {
    return 3;
  }

  auto r = prng::seed<prng::xorshift32>();
  fprintf(stderr, "sizeof(dht::DHT[%zu])\n", sizeof(dht::DHT));
  auto mdht = std::make_unique<dht::DHT>(udp_fd, listen, r, sp::now());
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

  fprintf(stderr, "node id: %s\n", to_hex(mdht->id));

  {
    char str[256] = {0};
    assertx(to_string(mdht->ip, str, sizeof(str)));
    fprintf(stderr, "remote(%s)\n", str);
    assertx(to_string(listen, str, sizeof(str)));
    fprintf(stderr, "bind(%s)\n", str);
  }

  dht::ModulesAwake modulesAwake;
  setup_epoll(*mdht, modulesAwake, options, udp_fd, signal_fd, priv_fd);

  dht::Modules modules{modulesAwake};
  if (!dht_upnp::setup(modules)) {
    die("dht_upnp::setup(modules)");
  }

  auto on_awake = [&mdht, &modulesAwake](sp::Buffer &out) -> sp::Milliseconds {
    // print_result(mdht->election);
    Timestamp next = mdht->now + mdht->config.refresh_interval;
    auto cb = [&mdht, &out](auto acum, auto callback) {
      Timestamp cr = callback(*mdht, out);
      assertx(cr > mdht->now);
      if (cr > mdht->now)
        return std::min(cr, acum);
      else
        return acum;
    };
    next = reduce(modulesAwake.on_awake, next, cb);
    assertx(next > mdht->now);

    logger::awake::timeout(*mdht, next);
    mdht->last_activity = mdht->now;

    return sp::Milliseconds(next) - sp::Milliseconds(mdht->now);
  };

  return main_loop(*mdht, on_awake);
}
