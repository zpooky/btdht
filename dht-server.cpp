#define _GNU_SOURCE
#define GNU_SOURCE
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
#include <sys/stat.h>
#include <udp.h>
#include <unistd.h> //read

#include <encode/hex.h>

#include <bencode.h>
#include <cache.h>
#include <core.h>
#include <dht_interface.h>
#include <private_interface.h>
#include <scrape.h>
#include <upnp_service.h>

// TODO in private_interface on socket close automatically close connected
// searches!
// TODO awake next timeout[0ms]
// TODO use ip not ip:port in bloomfilters
// TODO if both eth0 & wlan0 is active there is some problem

// TODO getopt: repeating bootstrap nodes

// TODO
// - cache tx raw sent and print when parse error response to file
// - find_response & others should be able to handle error response
// TODO BytesView implement mark
// TODO log explicit error response (error module)
// XXX ipv6
// XXX client: multiple receiver for the same search
// TODO replace bad node
//
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

    if (!interface_dht::setup(modules, true)) {
      die("interface_dht::setup(modules)");
    }
    core_cb.closure = this;
    core_cb.callback = on_dht_protocol_handle;
  }
};

static bool
parse(dht::Domain dom, dht::DHT &dht, dht::Modules &modules,
      const Contact &peer, sp::Buffer &in, sp::Buffer &out) noexcept {
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
      if (tx::consume_transaction(dht, pctx.tx, tctx)) {
        assertx((cnt - 1) == dht.client.active);
        logger::receive::res::known_tx(mctx, tctx);
        return tctx.handle(mctx);
      } else {
        logger::receive::res::unknown_tx(mctx, in);
        // assertx(false);
      }
    } else if (std::strcmp(pctx.msg_type, "e") == 0) { /*error*/
      tx::TxContext tctx;
      if (tx::consume_transaction(dht, pctx.tx, tctx)) {
        logger::receive::res::known_tx(mctx, tctx);
      } else {
        logger::receive::res::unknown_tx(mctx, in);
      }
      bencode_print_out(stderr);
      sp::Buffer copy(in);
      copy.pos = 0;
      // TODO print which request type that generated the error
      if (!bencode_print(copy)) {
        hex::encode_print(copy.raw, copy.length, stderr);
      }
      // assertx(false);
    } else {
      assertx(false);
    }

    return false;
  };

  krpc::ParseContext pctx(dom, dht, in);
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
        dht::Domain dom = dht::Domain::Domain_public;
        if (!parse(dom, self->dht, self->modules, from, inBuffer, outBuffer)) {
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
on_priv_protocol_ACCEPT_callback(void *closure, uint32_t events);

struct priv_protocol_ACCEPT_callback {
  sp::core_callback core_cb;
  dht::Modules modules;
  dht::DHT &dht;
  dht::Options &options;
  fd &priv_fd;

  priv_protocol_ACCEPT_callback(dht::ModulesAwake &awake, dht::DHT &_dht,
                                dht::Options &_options, fd &_fd)
      : core_cb{}
      , modules{awake}
      , dht{_dht}
      , options{_options}
      , priv_fd{_fd} {
    if (!interface_priv::setup(modules)) {
      die("interface_priv::setup(modules)");
    }
    if (!interface_dht::setup(modules, false)) {
      die("priv interface_dht::setup(modules)");
    }
    core_cb.closure = this;
    core_cb.callback = on_priv_protocol_ACCEPT_callback;
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
on_priv_protocol_ACCEPT_callback(void *closure, uint32_t events) {
  struct ucred ucred{};
  socklen_t len = sizeof(struct ucred);

  auto self = (priv_protocol_ACCEPT_callback *)closure;
  int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
  sp::fd client_fd{::accept4(int(self->priv_fd), NULL, NULL, flags)};

  if (!bool(client_fd)) {
    printf("priv accept4: %s", strerror(errno));
    return -1;
  }
  auto client = new priv_protocol_callback{self->modules, self->dht,
                                           self->options, std::move(client_fd)};
  if (getsockopt(int{client->client_fd}, SOL_SOCKET, SO_PEERCRED, &ucred,
                 &len) == 0) {
    printf("pid:%u, uid:%u, gid:%u\n", (unsigned)ucred.pid, ucred.uid,
           ucred.gid);
  } else {
    printf("%s\n", strerror(errno));
  }

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
          dht::Domain dom = dht::Domain::Domain_private;
          if (!parse(dom, self->dht, self->modules, from, inBuffer,
                     outBuffer)) {
            fprintf(stderr, "%s:parse error\n", __func__);
            events = EPOLLERR;
            goto Lout;
          }

          flip(outBuffer);
          fprintf(stderr, "%s: outBuffer.length:%zu\n", __func__,
                  outBuffer.length);
          if (outBuffer.length > 0) {
            if (!net::sock_write(self->client_fd, outBuffer)) {
              fprintf(stderr, "%s: fd:%d sock_write failed: %s (%d)\n",
                      __func__, int(self->client_fd), strerror(errno), errno);
            }
          }
        } else {
          res = 1;
        }
      } else {
        fprintf(stderr, "%s: fd:%d sock_read failed: %s (%d)\n", __func__,
                int(self->client_fd), strerror(errno), errno);
      }
    } // while
  }
Lout:
  if (events & EPOLLERR || events & EPOLLHUP || events & EPOLLRDHUP) {
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
            fd &udp_fd, fd &signal_fd, fd &priv_fd, fd &publish_fd) noexcept {
  auto dp_cb = new dht_protocol_callback{awake, self, options, udp_fd};
  auto pp_cb = new priv_protocol_ACCEPT_callback{awake, self, options, priv_fd};
  auto publish_cb = new dht::publish_ACCEPT_callback{&self, publish_fd};
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

  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
  ev.data.ptr = &publish_cb->core_cb;
  if (::epoll_ctl(self.core.epoll_fd, EPOLL_CTL_ADD, int(publish_fd), &ev) <
      0) {
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
  fprintf(stderr, "sizeof(DHT): %zuB %zuKB %zuMB\n", sizeof(dht::DHT),
          sizeof(dht::DHT) / 1024, sizeof(dht::DHT) / 1024 * 1024);
  pid_t pid = getpid();
  fprintf(stderr, "pid: %lu\n", (unsigned long)pid);
  std::srand((unsigned int)time(nullptr));

  fd signal_fd = setup_signal();
  if (!signal_fd) {
    return 2;
  }

  dht::Options options; // TODO large stack
  if (!dht::parse(options, argc, argv)) {
    return 1;
  }

  fd udp_fd = udp::bind_v4(options.port, udp::Mode::NONBLOCKING);
  if (!udp_fd) {
    fprintf(stderr, "failed to bind: %u\n", options.port);
    return 3;
  }

  umask(077);

  sp::fd priv_fd =
      udp::bind_unix_seq(options.local_socket, udp::Mode::NONBLOCKING);

  printf("%s (%d)\n", options.local_socket, int(priv_fd));
  if (!bool(priv_fd)) {
    fprintf(stderr, "failed to bind: local %s\n", options.local_socket);
    return 3;
  }

  sp::fd publish_fd =
      udp::bind_unix(options.publish_socket, udp::Mode::NONBLOCKING);
  printf("%s (%d)\n", options.publish_socket, int(publish_fd));
  if (!bool(publish_fd)) {
    fprintf(stderr, "failed to bind: publish %s\n", options.publish_socket);
    return 3;
  }

  Contact local_ip;
  if (!net::local(udp_fd, local_ip)) {
    return 3;
  }

  dht::Client client{udp_fd, priv_fd};

  sp_upnp *upnp = sp_upnp_new();

  auto r = prng::seed<prng::xorshift32>();
  fprintf(stderr, "sizeof(dht::DHT[%zu])\n", sizeof(dht::DHT));
  Timestamp now = sp::now();

  auto mdht =
      std::make_unique<dht::DHT>(local_ip, client, r, now, options, upnp);
  if (!dht::init(*mdht, options)) {
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

#if 0
  {
    char str[256] = {0};
    to_string(mdht->external_ip, str, sizeof(str));
    fprintf(stderr, "external ip(%s)\n", str);
  }


  // TODO wget from known
  to_contact("188.151.233.22:11000", mdht->external_ip);
  dht::randomize_NodeId(mdht->random, mdht->external_ip.ip, mdht->id);
  fprintf(stderr, "node id: %s (strict: %s)\n", to_hex(mdht->id),
          is_valid_strict_id(mdht->external_ip.ip, mdht->id) ? "true"
                                                             : "false");
#else
  if (upnp) {
    mdht->external_ip.ip = Ip(sp_upnp_external_ip(upnp));
  }

  dht::randomize_NodeId(mdht->random, mdht->external_ip.ip, mdht->id);
#endif

  fprintf(stderr, "node id: %s (strict: %s)\n", to_hex(mdht->id),
          is_valid_strict_id(mdht->external_ip.ip, mdht->id) ? "true"
                                                             : "false");
  {
    char str[256] = {0};
    to_string(mdht->external_ip, str, sizeof(str));
    fprintf(stderr, "external ip(%s)\n", str);
    to_string(local_ip, str, sizeof(str));
    fprintf(stderr, "local ip(%s)\n", str);
  }

  dht::ModulesAwake modulesAwake;
  setup_epoll(*mdht, modulesAwake, options, udp_fd, signal_fd, priv_fd,
              publish_fd);

  dht::Modules modules{modulesAwake};
  if (!dht_upnp::setup(modules)) {
    die("dht_upnp::setup(modules)");
  }
  if (!interface_setup::setup(modules, true)) {
    die("interface_setup::setup(modules)");
  }

  auto on_awake = [&mdht, &modulesAwake](sp::Buffer &out) -> sp::Milliseconds {
    // print_result(mdht->election);
    Timestamp next{mdht->now + mdht->config.refresh_interval};
    auto cb = [&mdht, &out](auto acum, auto callback) {
      Timestamp cr = callback(*mdht, out);
      assertxs(cr > mdht->now, std::uint64_t(cr), std::uint64_t(mdht->now));
      if (cr > mdht->now)
        return std::min(cr, acum);
      else
        return acum;
    };
    shuffle(mdht->random, modulesAwake.on_awake);
    next = reduce(modulesAwake.on_awake, next, cb);
    assertx(next > mdht->now);

    logger::awake::timeout(*mdht, next);
    mdht->last_activity = mdht->now;

    return sp::Milliseconds(next) - sp::Milliseconds(mdht->now);
  };

  int res = main_loop(*mdht, on_awake);

  if (upnp) {
    if (mdht->upnp_external_port) {
      sp_upnp_delete_port_mapping(upnp, mdht->upnp_external_port, "UDP");
    }
  }
  sp_upnp_free(&upnp);
  return res;
}
