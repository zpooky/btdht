#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"

// #Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
void
randomize(NodeId &) noexcept;

/*TokenPair*/
struct TokenPair {
  Ipv4 ip;
  Token token;
  time_t created;

  TokenPair();
  operator bool() const noexcept;
};


bool
is_good(DHT &, const Node &) noexcept;

bool
init(dht::DHT &) noexcept;

void
mintToken(DHT &, Ipv4, Token &, time_t) noexcept;

bool
is_blacklisted(DHT &dht, const dht::Contact &) noexcept;

/**/
void
find_closest(DHT &, const NodeId &, Node *(&)[Bucket::K]) noexcept;

void
find_closest(DHT &, const Infohash &, Node *(&)[Bucket::K]) noexcept;

bool
valid(DHT &, const krpc::Transaction &) noexcept;

Node *
find_contact(DHT &, const NodeId &) noexcept;

Node *
insert(DHT &, const Node &) noexcept;

} // namespace dht

namespace timeout {
void
unlink(dht::DHT &, dht::Node *) noexcept;

void
append_all(dht::DHT &, dht::Node *) noexcept;
} // namespace timeout

namespace lookup {
/**/
dht::KeyValue *
lookup(dht::DHT &, const dht::Infohash &, time_t) noexcept;

bool
insert(dht::DHT &, const dht::Infohash &, const dht::Contact &,
       time_t) noexcept;

bool
valid(dht::DHT &, const dht::Token &) noexcept;
} // namespace lookup

#endif
