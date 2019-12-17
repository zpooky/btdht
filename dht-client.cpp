#include <bencode_print.h>
#include <dht.h>
#include <dslbencode.h>
#include <encode/hex.h>
#include <getopt.h>
#include <krpc.h>
#include <prng/URandom.h>
#include <prng/util.h>
#include <signal.h>
#include <stdio.h>
#include <sys/epoll.h>    //epoll
#include <sys/signalfd.h> //signalfd
#include <tcp.h>
#include <udp.h>
#include <upnp.h>

struct DHTClient {
  int argc;
  char **argv;
  sp::Buffer &in;
  sp::Buffer &out;
  prng::URandom &rand;
  fd &udp;
  dht::NodeId self;

  DHTClient(int ac, char **as, sp::Buffer &i, sp::Buffer &o, prng::URandom &r,
            fd &u)
      : argc(ac)
      , argv(as)
      , in(i)
      , out(o)
      , rand(r)
      , udp(u)
      , self() {
  }
};

static void
randomize(prng::URandom &r, dht::NodeId &id) noexcept {
  fill(r, id.id);
  for (std::size_t i = 0; i < 3; ++i) {
    auto pre = uniform_dist(r, std::uint32_t(0), std::uint32_t(9));
    assertxs(pre <= 9, pre);
    id.id[i] = sp::byte(pre);
  }
}

static fd
setup_signal() {
  /* Fetch current signal mask */
  sigset_t sigset;
  if (sigprocmask(SIG_SETMASK, NULL, &sigset) < 0) {
    return fd{-1};
  }

  /* Block signals so that they aren't handled
   * according to their default dispositions
   */
  sigfillset(&sigset);

  /*job control*/
  sigdelset(&sigset, SIGCONT);
  sigdelset(&sigset, SIGTSTP);

  /* Modify signal mask */
  if (sigprocmask(SIG_SETMASK, &sigset, NULL) < 0) {
    return fd{-1};
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
    return fd{-1};
  }

  return fd{sfd};
}

static bool
parse_contact(DHTClient &client, Contact &c) noexcept {
  if (client.argc >= 1) {
    const char *p = client.argv[0];
    if (to_contact(p, c)) {
      client.argc--;
      client.argv++;
      return true;
    }
  }
  return false;
}

static bool
parse_id(DHTClient &client, dht::Infohash &id) {
  if (client.argc >= 1) {
    if (from_hex(id, client.argv[0])) {
      client.argc--;
      client.argv++;
      return true;
    }
  }
  return false;
}

static bool
parse_id(DHTClient &client, dht::NodeId &id) {
  if (client.argc >= 1) {
    if (from_hex(id, client.argv[0])) {
      client.argc--;
      client.argv++;
      return true;
    }
  }
  return false;
}

static bool
generic_receive(fd &u, sp::Buffer &b) noexcept {
Lretry:
  reset(b);

  Contact c;
  int res = udp::receive(u, c, b);
  if (res != 0) {
    if (res == -EAGAIN) {
      goto Lretry;
    }
    return false;
  }
  flip(b);

  dht::print_hex(b.raw, b.length, stdout);
  printf("\n");
  bencode_print(b);
  return true;
}

template <typename R>
static void
make_tx(R &r, krpc::Transaction &tx) {
  prng::fill(r, tx.id);
  tx.length = 5;
}
template <typename R>

static void
make_t(R &r, dht::Token &t) {
  prng::fill(r, t.id);
  t.length = 5;
}
/*
 * # Statistics
 */
static void
send_statistics(prng::URandom &r, fd &udp, const Contact &to,
                sp::Buffer &b) noexcept {
  reset(b);
  krpc::Transaction t;
  make_tx(r, t);
  krpc::request::statistics(b, t);
  flip(b);

  udp::send(udp, to, b);
}

static void
receive_statistics(fd &u, sp::Buffer &b) noexcept {
  printf("#receive statistics\n");
  generic_receive(u, b);
  printf("\n\n");
}
/*
 * # Dump
 */
static void
send_dump(prng::URandom &r, fd &udp, const Contact &to,
          sp::Buffer &b) noexcept {
  reset(b);
  krpc::Transaction t;
  make_tx(r, t);
  krpc::request::dump(b, t);
  flip(b);

  udp::send(udp, to, b);
}

static void
receive_dump(fd &u, sp::Buffer &b) noexcept {
  printf("#receive dump\n");
  generic_receive(u, b);
  printf("\n\n");
}

//====================================================
static bool
send_search(DHTClient &client, const Contact &to,
            const dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::request::search(client.out, tx, search, 6000000);
  flip(client.out);

  return udp::send(client.udp, to, client.out);
}

static bool
send_stop_seach(DHTClient &client, const Contact &to,
                const dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::request::stop_search(client.out, tx, search);
  flip(client.out);

  return udp::send(client.udp, to, client.out);
}

//====================================================
static void
send_ping(DHTClient &client, const Contact &to) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);
  krpc::request::ping(client.out, tx, client.self);
  flip(client.out);

  udp::send(client.udp, to, client.out);
}

static void
receive_ping(fd &u, sp::Buffer &b) noexcept {
  printf("#receive ping\n");
  generic_receive(u, b);
  printf("\n\n");
}

//========
static bool
send_find_node(DHTClient &client, const Contact &to,
               const dht::NodeId &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::request::find_node(client.out, tx, client.self, search);
  flip(client.out);

  return udp::send(client.udp, to, client.out);
}

//=====================================
/* # get_peers */
static void
send_get_peers(DHTClient &client, const Contact &to,
               dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::request::get_peers(client.out, tx, client.self, search);
  flip(client.out);
  udp::send(client.udp, to, client.out);
}

static dht::Token
receive_get_peers(fd &u, sp::Buffer &b) noexcept {
  printf("#receive get_peers\n");
  generic_receive(u, b);
  printf("\n\n");

  dht::Token tx;
  // assertx(find_entry(b, "token", tx.id, tx.length));
  return tx;
}

//=====================================
/* # announce_peer */
static void
send_announce_peer(DHTClient &client, const Contact &to, dht::Token &token,
                   dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);
  bool implied_port = true;
  krpc::request::announce_peer(client.out, tx, client.self, implied_port,
                               search, 0, token);
  flip(client.out);

  udp::send(client.udp, to, client.out);
}

static void
receive_announce_peer(fd &u, sp::Buffer &b) noexcept {
  printf("#receive announce_peer\n");
  generic_receive(u, b);
  printf("\n\n");
}

int
handle_ping(DHTClient &client) {
  // dht::Infohash search;
  // prng::fill(r, search.id);

  // Contact to;
  // if (!convert(avrgs[1], to)) {
  //   printf("parse dest-ip[%s] failed\n", args[1]);
  //   return 1;
  // }
  //
  // send_statistics(r, udp, to, outBuffer);
  // receive_statistics(udp, inBuffer);
  return 0;
}

int
handle_find_node(DHTClient &client) {
  bool has = false;
  Contact to;
  dht::NodeId search;
  // printf("%s\n", client.argv[0]);

  while (1) {
    int opt;
    int option_index = 0;
    static struct option loptions[] = {
        //
        {"self", required_argument, 0, 's'},
        {0, 0, 0, 0}
        //
    };

    if (!has) {
      auto argc = client.argc;
      auto argv = client.argv;
      if (parse_contact(client, to)) {
        if (parse_id(client, search)) {
          has = true;
        }
      }

      if (!has) {
        client.argc = argc;
        client.argv = argv;
      }
    }

    opt = getopt_long(client.argc, client.argv, "s:h", loptions, &option_index);
    if (opt == -1) {
      break;
    }

    switch (opt) {
    case 's':
      printf("-s \n");
      // TODO
      break;
    case 'h':
      printf("-h \n");
      return EXIT_SUCCESS;
      break;
    default:
      return EXIT_FAILURE;
    }
  }

  if (!has) {
    fprintf(stderr, "dht-client find_node ip:port search [--self=hex]\n");
    return EXIT_FAILURE;
  }

  if (!send_find_node(client, to, search)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

//=====================================
int
handle_get_peers(DHTClient &client) {
  return 0;
}

//=====================================
int
handle_announce_peer(DHTClient &client) {
  return 0;
}

//=====================================
int
handle_statistics(DHTClient &client) {
  return 0;
}

//=====================================
static bool
search_event_receive(fd &u, sp::Buffer &b) noexcept {
  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);
  Contact remote;

Lretry:
  reset(b);
  int res = udp::receive(u, remote, b);
  if (res != 0) {
    if (res == -EAGAIN) {
      goto Lretry;
    }
    return false;
  }
  flip(b);

  krpc::ParseContext pctx(dht, b);

  // dht::print_hex(b.raw, b.length);
  // printf("\n");
  return krpc::d::krpc(pctx, [](krpc::ParseContext &ctx) { //
    dht::Infohash search;
    sp::UinStaticArray<Contact, 128> contacts;

    return bencode::d::dict(ctx.decoder, [&](sp::Buffer &p) {
      if (!bencode::d::pair(p, "id", search.id)) {
        return false;
      }

      if (!bencode::d::value(p, "contacts")) {
        return false;
      } else {
        if (!bencode::d::priv::value(p, contacts)) {
          return false;
        }
      }

      for_each(contacts, [](auto &c) {
        char buffer[128] = {'\0'};
        assertx_n(to_string(c, buffer));
        printf("%s\n", buffer);
      });

      return true;
    });
  });
}

int
handle_search(DHTClient &client) {
  bool has = false;
  Contact to;
  dht::Infohash search;

  while (1) {
    int opt;
    int option_index = 0;
    static struct option loptions[] = {
        //
        {"self", required_argument, 0, 's'},
        {0, 0, 0, 0}
        //
    };

    if (!has) {
      auto argc = client.argc;
      auto argv = client.argv;
      if (parse_contact(client, to)) {
        if (parse_id(client, search)) {
          has = true;
        }
      }

      if (!has) {
        client.argc = argc;
        client.argv = argv;
      }
    }

    opt = getopt_long(client.argc, client.argv, "s:h", loptions, &option_index);
    if (opt == -1) {
      break;
    }

    switch (opt) {
    case 's':
      printf("-s \n");
      // TODO
      break;
    case 'h':
      printf("-h \n");
      return EXIT_SUCCESS;
      break;
    default:
      return EXIT_FAILURE;
    }
  }

  if (!has) {
    fprintf(stderr, "dht-client search ip:port search [--self=hex]\n");
    return EXIT_FAILURE;
  }

  if (!send_search(client, to, search)) {
    return EXIT_FAILURE;
  }

  fd pfd{::epoll_create1(0)};
  if (!pfd) {
    return EXIT_FAILURE;
  }

  ::epoll_event ev;

  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
  ev.data.fd = int(client.udp);
  if (::epoll_ctl(int(pfd), EPOLL_CTL_ADD, int(client.udp), &ev) < 0) {
    return EXIT_FAILURE;
  }

  fd sig_fd{setup_signal()};
  if (!sig_fd) {
    return EXIT_FAILURE;
  }

  ev.events = EPOLLIN;
  ev.data.fd = int(sig_fd);
  if (::epoll_ctl(int(pfd), EPOLL_CTL_ADD, int(sig_fd), &ev) < 0) {
    return EXIT_FAILURE;
  }

  bool first = true;
  while (true) {
    ::epoll_event events;

    int timeout = -1;
    int no_events = ::epoll_wait(int(pfd), &events, 1, timeout);
    if (no_events <= 0) {
      if (errno == EAGAIN) {
        continue;
      } else {
        printf("break[%d] [%s]\n", no_events, strerror(errno));
        break;
      }
    }

    if (events.data.fd == int(client.udp)) {
      if (first) {
        if (!generic_receive(client.udp, client.in)) {
          printf("receive failed 1\n");
          break;
        }
        first = false;
      } else {
        if (!search_event_receive(client.udp, client.in)) {
          printf("receive failed 2\n");
          break;
        }
      }
    } else if (events.data.fd == int(sig_fd)) {
      break;
    } else {
      printf("unknown\n");
      break;
    }
  } // while

  printf("send_stop_seach()\n");
  if (!send_stop_seach(client, to, search)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int
handle_dump(DHTClient &client) {
  return 0;
}

typedef int (*exe_cb)(DHTClient &);
static int
bind_exe(int argc, char **argv, exe_cb cb) noexcept {
  fd udp = udp::bind_v4(udp::Mode::BLOCKING);
  if (!udp) {
    fprintf(stderr, "failed bind\n");
    return 1;
  }

  Contact local;
  if (!udp::local(udp, local)) {
    fprintf(stderr, "failed local\n");
    return 1;
  }

  prng::URandom r;

  constexpr std::size_t size = 12 * 1024 * 1024;

  auto in = new sp::byte[size];
  auto out = new sp::byte[size];

  sp::Buffer inBuffer(in, size);
  sp::Buffer outBuffer(out, size);

  DHTClient client(argc, argv, inBuffer, outBuffer, r, udp);
  randomize(r, client.self);
  int res = cb(client);

  delete[] in;
  delete[] out;

  return res;
}

static int
handle_upnp(int, char **) noexcept {
  Contact gateway;
  // http://192.168.1.1:48353/ctl/IPConn
  if (!to_contact("192.168.1.1:48353", gateway)) {
    return 1;
  }

  fd tcp = tcp::connect(gateway, tcp::Mode::BLOCKING);
  if (!tcp) {
    return 2;
  }

  Contact local;
  if (!tcp::local(tcp, local)) {
    return 5;
  }
  upnp::upnp in{sp::Seconds(sp::Minutes(5))};
  in.protocol = "udp";
  in.local = 42605;
  in.external = in.local;
  in.ip = local.ip;

  if (!upnp::http_add_port(tcp, in)) {
    return 4;
  }

  sp::StaticBytesView<1024 * 2> buf;
  if (tcp::read(tcp, buf) != 0) {
    return 3;
  }
  flip(buf);
  printf("'%.*s': %zu\n", buf.length, buf.raw, buf.length);

  return 0;
}

static int
parse_command(int argc, char **argv) {
  if (argc >= 2) {
    const char *subcommand = argv[1];

    int subc = argc - 1 - 1;
    char **subv = argv + 1 + 1;

    if (std::strcmp(subcommand, "ping") == 0) {
      return bind_exe(subc, subv, handle_ping);
    } else if (std::strcmp(subcommand, "find_node") == 0) {
      return bind_exe(subc, subv, handle_find_node);
    } else if (std::strcmp(subcommand, "get_peers") == 0) {
      return bind_exe(subc, subv, handle_get_peers);
    } else if (std::strcmp(subcommand, "announce_peer") == 0) {
      return bind_exe(subc, subv, handle_announce_peer);
    } else if (std::strcmp(subcommand, "statistics") == 0) {
      return bind_exe(subc, subv, handle_statistics);
    } else if (std::strcmp(subcommand, "search") == 0) {
      return bind_exe(subc, subv, handle_search);
    } else if (std::strcmp(subcommand, "dump") == 0) {
      return bind_exe(subc, subv, handle_dump);
    } else if (std::strcmp(subcommand, "upnp") == 0) {
      return handle_upnp(subc, subv);
    } else {
      fprintf(stderr, "unknown subcommand '%s'\n", subcommand);
      return EXIT_FAILURE;
    }
  }

  fprintf(stderr, "missing subcommand\n");
  return EXIT_FAILURE;
}

//===================================================
int
main(int argc, char **args) {
  return parse_command(argc, args);
}
