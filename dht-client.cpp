#include "decode_bencode.h"
#include "priv_decode_bencode.h"
#include <bencode_print.h>
#include <dht.h>
#include <encode/hex.h>
#include <errno.h>
#include <getopt.h>
#include <krpc.h>
#include <krpc_parse.h>
#include <memory>
#include <priv_krpc.h>
#include <prng/URandom.h>
#include <prng/util.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>    //epoll
#include <sys/signalfd.h> //signalfd
#include <tcp.h>
#include <udp.h>
#include <upnp.h>
#include <upnp_miniupnp.h>

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
generic_receive(fd &u, sp::Buffer &b, bool print_hex = false) noexcept {
Lretry:
  reset(b);

  Contact c;
  int res = net::sock_read(u, b);
  if (res != 0) {
    if (res == -EAGAIN) {
      goto Lretry;
    }
    return false;
  }
  flip(b);

  if (print_hex) {
    printf("response: \n");
    dht::print_hex(stdout, b.raw, b.length);
    printf("\n");
  }
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

//====================================================
static bool
send_statistics(DHTClient &client) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::priv::request::statistics(client.out, tx);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

static bool
send_dump(DHTClient &client) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::priv::request::dump(client.out, tx);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

static bool
send_dump_scrape(DHTClient &client) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::priv::request::dump_scrape(client.out, tx);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

static bool
send_dump_db(DHTClient &client) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::priv::request::dump_db(client.out, tx);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

//====================================================
static bool
send_search(DHTClient &client, const dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::priv::request::search(client.out, tx, search, 6000000);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

static bool
send_stop_seach(DHTClient &client, const dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::priv::request::stop_search(client.out, tx, search);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

//====================================================
static int
send_ping(DHTClient &client) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);
  krpc::request::ping(client.out, tx, client.self);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

//========
static bool
send_find_node(DHTClient &client, const dht::NodeId &search) noexcept {
  bool n4 = true;
  bool n6 = false;
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::request::find_node(client.out, tx, client.self, search, n4, n6);
  flip(client.out);

  return net::sock_write(client.udp, client.out);
}

//=====================================
/* # get_peers */
static bool
send_get_peers(DHTClient &client, const dht::Infohash &search) noexcept {
  bool n4 = true;
  bool n6 = false;
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  krpc::request::get_peers(client.out, tx, client.self, search, n4, n6);
  flip(client.out);
  return net::sock_write(client.udp, client.out);
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
send_announce_peer(DHTClient &client, dht::Token &token,
                   dht::Infohash &search) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);
  bool implied_port = true;
  krpc::request::announce_peer(client.out, tx, client.self, implied_port,
                               search, 0, token);
  flip(client.out);

  net::sock_write(client.udp, client.out);
}

static void
receive_announce_peer(fd &u, sp::Buffer &b) noexcept {
  printf("#receive announce_peer\n");
  generic_receive(u, b);
  printf("\n\n");
}

int
handle_ping(DHTClient &client) {
  bool has = false;
  Contact to;

  if (!send_ping(client)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return 0;
}

int
handle_find_node(DHTClient &client) {
  dht::NodeId search;
  // printf("%s\n", client.argv[0]);

  while (1) {
    int opt;
    int option_index = 0;
    static struct option loptions[] = {
        //
        {"self", required_argument, 0, 's'},
        {0, 0, 0, 0} //
    };

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

  if (!send_find_node(client, search)) {
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
  dht::Infohash search;
  // printf("%s\n", client.argv[0]);

  while (1) {
    int opt;
    int option_index = 0;
    static struct option loptions[] = {
        //
        {"self", required_argument, 0, 's'},
        {0, 0, 0, 0} //
    };

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

  if (!send_get_peers(client, search)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
  return 0;
}

//=====================================
int
handle_announce_peer(DHTClient &client) {
  return 0;
}

//=====================================
static bool
sample_infohashes_receive(fd &u, sp::Buffer &b) noexcept {
  fd udp{-1};
  fd priv{-1};
  Contact listen;
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::Client client{udp, priv};
  dht::Options opt;
  auto mdht = std::make_unique<dht::DHT>(listen, client, r, now, opt);
  Contact remote;

Lretry:
  reset(b);
  int res = net::sock_read(u, b);
  if (res != 0) {
    if (res == -EAGAIN) {
      goto Lretry;
    }
    return false;
  }
  flip(b);

  dht::Domain dom = dht::Domain::Domain_private;
  krpc::ParseContext pctx(dom, *mdht, b);

  // dht::print_hex(b.raw, b.length);
  // printf("\n");
  krpc::SampleInfohashesResponse response;
  bool krpc_res = krpc::d::krpc(pctx, [&response](krpc::ParseContext &p) { //
    return krpc::parse_sample_infohashes_response(p.decoder, response);
  });

  if (!krpc_res) {
    fprintf(stderr, "parse error\n");
    return false;
  }

  printf("id: ");
  dht::print_hex(stdout, response.id);
  printf("interval: %u\n", response.interval);
  printf("num: %u\n", response.num);
  printf("samples:\n");
  for_each(response.samples, [](auto &ih) {
    printf("\t");
    dht::print_hex(stdout, ih);
  });
  printf("nodes:\n");
  for (auto &node : response.nodes) {
    auto id = std::get<0>(node);
    auto contact = std::get<1>(node);
    printf("\t%s:", to_string(contact));
    dht::print_hex(stdout, id);
  }

  return true;
}

static bool
send_sample_infohashes(DHTClient &client, const dht::Key &target) noexcept {
  reset(client.out);
  krpc::Transaction tx;
  make_tx(client.rand, tx);

  bool n4 = true;
  bool n6 = false;
  krpc::request::sample_infohashes(client.out, tx, client.self, target, n4, n6);
  flip(client.out);
  std::size_t pos = client.out.pos;
  bencode_print(client.out);
  client.out.pos = pos;

  return net::sock_write(client.udp, client.out);
}

int
handle_sample_infohashes(DHTClient &client) {

  printf("target: ");
  dht::print_hex(stdout, client.self);
  if (!send_sample_infohashes(client, client.self.id)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }
  printf("--------------------\n");
#if 1
  if (!sample_infohashes_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }
#else

  if (!generic_receive(client.udp, client.in, true)) {
    return EXIT_FAILURE;
  }
#endif

  return 0;
}

//=====================================
int
handle_statistics(DHTClient &client) {
  if (!send_statistics(client)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return 0;
}

//=====================================
static bool
search_event_receive(fd &u, sp::Buffer &b) noexcept {
  fd udp{-1};
  fd priv{-1};
  Contact listen;
  prng::xorshift32 r(1);
  Timestamp now = sp::now();

  dht::Client client{udp, priv};
  dht::Options opt;
  dht::DHT dht(listen, client, r, now, opt);
  Contact remote;

Lretry:
  reset(b);
  int res = net::sock_read(u, b);
  if (res != 0) {
    if (res == -EAGAIN) {
      goto Lretry;
    }
    return false;
  }
  flip(b);

  dht::Domain dom = dht::Domain::Domain_private;
  krpc::ParseContext pctx(dom, dht, b);

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
        if (!bencode_priv_d<sp::Buffer>::value(p, contacts)) {
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
  dht::Infohash search;

  while (1) {
    int opt;
    int option_index = 0;
    static struct option loptions[] = {
        //
        {"self", required_argument, 0, 's'},
        {0, 0, 0, 0} //
    };

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
      fprintf(stderr, "dht-client search <infohash>\n");
      return EXIT_SUCCESS;
      break;
    default:
      return EXIT_FAILURE;
    }
  }

  if (!send_search(client, search)) {
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
  if (!send_stop_seach(client, search)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int
handle_dump(DHTClient &client) {
  if (!send_dump(client)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return 0;
}

static int
handle_dump_scrape(DHTClient &client) {
  if (!send_dump_scrape(client)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return 0;
}

static int handle_dump_db(DHTClient &client) {
  if (!send_dump_db(client)) {
    fprintf(stderr, "failed to send\n");
    return EXIT_FAILURE;
  }

  if (generic_receive(client.udp, client.in)) {
    return EXIT_FAILURE;
  }

  return 0;
}

typedef int (*exe_cb)(DHTClient &);
static int
bind_exe(const char *exe, const char *command, int argc, char **argv,
         exe_cb cb) noexcept {
  prng::URandom r;
  bool has = false;
  Contact to;

  constexpr std::size_t size = 12 * 1024 * 1024;

  auto in = new sp::byte[size];
  auto out = new sp::byte[size];

  sp::Buffer inBuffer(in, size);
  sp::Buffer outBuffer(out, size);

  while (argc) {
    const char *p = argv[0];
    if (to_contact(p, to)) {
      has = true;
      break;
    }

    argc--;
    argv++;
  }

  if (!has) {
    fprintf(stderr, "%s %s ip:port\n", exe, command);
    return EXIT_FAILURE;
  }

  fd udp = udp::connect(to.ip.ipv4, to.port, udp::Mode::BLOCKING);
  if (!udp) {
    fprintf(stderr, "failed connect: %s (%s)\n", to_string(to),
            strerror(errno));
    return EXIT_FAILURE;
  }
  DHTClient client(argc, argv, inBuffer, outBuffer, r, udp);
  randomize(r, client.self);
  int res = cb(client);

  delete[] in;
  delete[] out;

  return res;
}

static int
bind_priv_exe(const char *exe, const char *command, int argc, char **argv,
              exe_cb cb) noexcept {
  char local_socket[PATH_MAX]{0};
  if (!xdg_runtime_dir(local_socket)) {
    return 1;
  }
  ::strcat(local_socket, "/spdht.socket");

  fd udp = udp::connect_unix_seq(local_socket, udp::Mode::BLOCKING);
  if (!udp) {
    fprintf(stderr, "failed bind\n");
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
#if 0
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
  assertx(false); // TODO
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
  printf("'%.*s': %zu\n", (int)buf.length, buf.raw, buf.length);
#else

  struct sp_upnp *self = sp_upnp_new();
  struct sockaddr_in local{};
  uint32_t leaseDuration = 60;
  const char *proto = "TCP";
  Contact tmp;
  char buf[64]{};
  assertx(self);
  assertx(to_contact("192.168.0.3:1337", tmp));
  assertx(to_sockaddr(tmp, local));
  uint16_t external_port = 0;
  assertx(sp_upnp_create_port_mapping(self, local, &external_port, proto,
                                      &leaseDuration, "sp-test") == 0);
  sp_upnp_delete_port_mapping(self, (uint16_t)external_port, proto);
  to_string(sp_upnp_local_ip(self), buf, sizeof(buf));
  printf("local: %s\n", buf);
  to_string(sp_upnp_external_ip(self), buf, sizeof(buf));
  printf("external: %s\n", buf);

  sp_upnp_free(&self);
#endif

  return 0;
}

static int
parse_command(int argc, char **argv) {
  if (argc >= 2) {
    const char *exe = argv[0];
    const char *subcommand = argv[1];

    int subc = argc - 1 - 1;
    char **subv = argv + 1 + 1;

    if (std::strcmp(subcommand, "ping") == 0) {
      return bind_exe(exe, subcommand, subc, subv, handle_ping);
    } else if (std::strcmp(subcommand, "find_node") == 0) {
      return bind_exe(exe, subcommand, subc, subv, handle_find_node);
    } else if (std::strcmp(subcommand, "get_peers") == 0) {
      return bind_exe(exe, subcommand, subc, subv, handle_get_peers);
    } else if (std::strcmp(subcommand, "priv_get_peers") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv, handle_get_peers);
    } else if (std::strcmp(subcommand, "announce_peer") == 0) {
      return bind_exe(exe, subcommand, subc, subv, handle_announce_peer);
    } else if (std::strcmp(subcommand, "sample_infohashes") == 0) {
      return bind_exe(exe, subcommand, subc, subv, handle_sample_infohashes);
    } else if (std::strcmp(subcommand, "priv_sample_infohashes") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv,
                           handle_sample_infohashes);
    } else if (std::strcmp(subcommand, "statistics") == 0 ||
               std::strcmp(subcommand, "stat") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv, handle_statistics);
    } else if (std::strcmp(subcommand, "search") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv, handle_search);
    } else if (std::strcmp(subcommand, "dump") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv, handle_dump);
    } else if (std::strcmp(subcommand, "dump_scrape") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv, handle_dump_scrape);
    } else if (std::strcmp(subcommand, "dump_db") == 0) {
      return bind_priv_exe(exe, subcommand, subc, subv, handle_dump_db);
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
