#include <bencode_print.h>
#include <dht.h>
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

static void
send_ping(prng::Xorshift32 &r, fd &udp, const Contact &to,
          sp::Buffer &b) noexcept {
  reset(b);
  krpc::Transaction t;
  prng::fill(r, t.id);
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
static void
send_find_node(prng::Xorshift32 &r, fd &udp, const Contact &to,
               sp::Buffer &b) noexcept {
  reset(b);
  krpc::Transaction t;
  prng::fill(r, t.id);
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
static void
send_get_peers(prng::Xorshift32 &r, fd &udp, const Contact &to, sp::Buffer &b,
               dht::Infohash &search) noexcept {
  reset(b);
  krpc::Transaction t;
  prng::fill(r, t.id);

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
static void
send_announce_peer(prng::Xorshift32 &r, fd &udp, const Contact &to,
                   sp::Buffer &b, dht::Token &token,
                   dht::Infohash &search) noexcept {
  reset(b);
  krpc::Transaction t;
  prng::fill(r, t.id);
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

int
main(int argc, char **args) {
  fd udp = udp::bind(40025, udp::Mode::BLOCKING);
  if (!bool(udp)) {
    printf("failed bind\n");
    return 1;
  }

  Contact local = udp::local(udp);

  if (!randomize(local, self)) {
    printf("failed randomize node id\n");
    return 1;
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
  prng::Xorshift32 r(1337);

  sp::byte in[2048];
  sp::byte out[2048];
  sp::Buffer inBuffer(in);
  sp::Buffer outBuffer(out);

  dht::Infohash search;
  prng::fill(r, search.id);

  send_find_node(r, udp, to, outBuffer);
  receive_find_node(udp, inBuffer);

  send_ping(r, udp, to, outBuffer);
  receive_ping(udp, inBuffer);

  send_get_peers(r, udp, to, outBuffer, search);
  dht::Token token = receive_get_peers(udp, inBuffer);

  send_announce_peer(r, udp, to, outBuffer, token, search);
  receive_announce_peer(udp, inBuffer);

  return 0;
}
