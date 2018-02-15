#include <bencode_print.h>
#include <dht.h>
#include <encode/hex.h>
#include <krpc.h>
#include <prng/util.h>
#include <prng/xorshift.h>
#include <stdio.h>
#include <udp.h>

static dht::NodeId self;

static void
generic_receive(fd &u, sp::Buffer &b) noexcept {
  reset(b);

  Contact c;
  udp::receive(u, c, b);
  flip(b);

  char str[256] = {'\0'};
  assert(to_string(c, str));
  printf("contact: %s\n", str);
  // assert(b.length > 0);

  bencode::d::Decoder d(b);
  sp::bencode_print(d);
}

template <typename R>
static void
make_tx(R &r, krpc::Transaction &t) {
  prng::fill(r, t.id);
  t.length = 5;
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
send_statistics(prng::Xorshift32 &r, fd &udp, const Contact &to,
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
send_dump(prng::Xorshift32 &r, fd &udp, const Contact &to,
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
/*
 * # Ping
 */

static void
send_ping(prng::Xorshift32 &r, fd &udp, const Contact &to,
          sp::Buffer &b) noexcept {
  reset(b);
  krpc::Transaction t;
  make_tx(r, t);
  krpc::request::ping(b, t, self);
  flip(b);

  udp::send(udp, to, b);
}

static void
receive_ping(fd &u, sp::Buffer &b) noexcept {
  printf("#receive ping\n");
  generic_receive(u, b);
  printf("\n\n");
}

//========
/*
 * # find_node
 */
static void
send_find_node(prng::Xorshift32 &r, fd &udp, const Contact &to,
               sp::Buffer &b) noexcept {
  reset(b);
  krpc::Transaction t;
  make_tx(r, t);
  krpc::request::find_node(b, t, self, self);
  flip(b);

  udp::send(udp, to, b);
}

static void
receive_find_node(fd &u, sp::Buffer &b) noexcept {
  printf("#receive find_node\n");
  generic_receive(u, b);
  printf("\n\n");
}

//========
/*
 * # get_peers
 */
static void
send_get_peers(prng::Xorshift32 &r, fd &udp, const Contact &to, sp::Buffer &b,
               dht::Infohash &search) noexcept {
  reset(b);
  krpc::Transaction t;
  make_tx(r, t);

  krpc::request::get_peers(b, t, self, search);
  flip(b);
  udp::send(udp, to, b);
}

static dht::Token
receive_get_peers(fd &u, sp::Buffer &b) noexcept {
  printf("#receive get_peers\n");
  generic_receive(u, b);
  printf("\n\n");

  dht::Token t;
  assert(find_entry(b, "token", t.id, t.length));
  return t;
}

//========
/*
 * # announce_peer
 */
static void
send_announce_peer(prng::Xorshift32 &r, fd &udp, const Contact &to,
                   sp::Buffer &b, dht::Token &token,
                   dht::Infohash &search) noexcept {
  reset(b);
  krpc::Transaction t;
  make_tx(r, t);
  bool implied_port = true;
  krpc::request::announce_peer(b, t, self, implied_port, search, 0, token);
  flip(b);

  udp::send(udp, to, b);
}

static void
receive_announce_peer(fd &u, sp::Buffer &b) noexcept {
  printf("#receive announce_peer\n");
  generic_receive(u, b);
  printf("\n\n");
}
//===================================================

int
main(int argc, char **args) {
  fd udp = udp::bind(40025, udp::Mode::BLOCKING);
  if (!bool(udp)) {
    printf("failed bind\n");
    return 1;
  }

  Contact local = udp::local(udp);
  prng::Xorshift32 r(1337);
  // randomize(r, self);
  const char *hex = "6A2D049F1F1BBE9EBB3A6DB3C87C3E99245E52";
  std::size_t nlength = sizeof(self.id);
  printf("id[%s]\n", hex);
  assert(hex::decode(hex, self.id, nlength));
  {
    char str[256] = {0};
    std::size_t l = sizeof(str);
    hex::encode(self.id, sizeof(self.id), str, l);
    assert(strcmp(hex, str));
  }

  Contact to(0, 0);
  if (!(argc > 1)) {
    printf("missing dest ip\n");
    return 1;
  }
  if (!convert(args[1], to)) {
    printf("parse dest-ip[%s] failed\n", args[1]);
    return 1;
  }

  constexpr std::size_t size = 12 * 1024 * 1024;
  auto in = new sp::byte[size];
  auto out = new sp::byte[size];
  // sp::byte in[2048];
  // sp::byte out[2048];
  sp::Buffer inBuffer(in, size);
  sp::Buffer outBuffer(out, size);

  dht::Infohash search;
  prng::fill(r, search.id);

  // send_statistics(r, udp, to, outBuffer);
  // receive_statistics(udp, inBuffer);
  //
  // send_dump(r, udp, to, outBuffer);
  // receive_dump(udp, inBuffer);

  send_find_node(r, udp, to, outBuffer);
  receive_find_node(udp, inBuffer);

  send_ping(r, udp, to, outBuffer);
  receive_ping(udp, inBuffer);

  send_get_peers(r, udp, to, outBuffer, search);
  dht::Token token = receive_get_peers(udp, inBuffer);

  send_announce_peer(r, udp, to, outBuffer, token, search);
  receive_announce_peer(udp, inBuffer);

  delete[] in;
  delete[] out;

  return 0;
}
