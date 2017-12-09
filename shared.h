#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

#include "util.h"
#include <stdio.h> //debug
#include <stdlib.h>

#include "bencode.h"

//---------------------------
namespace krpc {
/*krpc::ParseContext*/
struct ParseContext {
  bencode::d::Decoder &decoder;
  Transaction tx;

  char msg_type[16];
  char query[16];
  sp::byte version[16];
  sp::byte ext_ip[16];

  //TODO move to src
  explicit ParseContext(bencode::d::Decoder &d) noexcept
      : decoder(d)
      , tx()
      , msg_type{0}
      , query{0}
      , version{0}
      , ext_ip{0} {
  }
};

} // namespace krpc

//---------------------------
namespace dht {
/*dht::Config*/
struct Config {
  time_t min_timeout_interval;
  time_t refresh_interval;
  time_t peer_age_refresh;
  time_t token_max_age;

  Config() noexcept;
};

/*dht::Infohash*/
struct Infohash {
  Key id;
  Infohash()
      : id{0} {
  }
};

bool
is_valid(const NodeId &) noexcept;

/*dht::Token*/
struct Token {
  sp::byte id[20];
  Token()
      : id{0} {
  }
};

/*dht::Peer*/
struct Peer {
  Contact contact;
  time_t activity;
  // {
  Peer *next;
  // }
  // {
  Peer *timeout_priv;
  Peer *timeout_next;
  // }
  Peer(Ipv4, Port, time_t) noexcept;
  Peer(const Contact &, time_t, Peer *next) noexcept;
  Peer() noexcept;

  bool
  operator==(const Contact &) const noexcept;
};

template <typename F>
static bool
for_all(const dht::Peer *l, F f) noexcept {
  while (l) {
    if (!f(*l)) {
      return false;
    }
    l = l->next;
  }
  return true;
}

time_t
activity(const Node &) noexcept;

/*dht::Bucket*/
struct Bucket {
  static constexpr std::size_t K = 8;
  Node contacts[K];

  Bucket();
  ~Bucket();
};

/*dht::RoutingTable*/
enum class NodeType { NODE, LEAF };
struct RoutingTable {
  union {
    struct {
      RoutingTable *higher;
      RoutingTable *lower;
      Key middle;
    } node;
    Bucket bucket;
  };
  NodeType type;

  RoutingTable(RoutingTable *h, RoutingTable *l);
  RoutingTable();

  ~RoutingTable();
};

/*dht::KeyValue*/
struct KeyValue {
  KeyValue *next;
  Peer *peers;
  Infohash id;
  //
  KeyValue(const Infohash &, KeyValue *);
};

/*dht::DHT*/
struct DHT {
  static const std::size_t token_table = 64;
  // self {{{
  NodeId id;
  //}}}
  // peer-lookup db {{{
  KeyValue *lookup_table;
  Token tokens[token_table];
  Peer *timeout_peer;
  time_t timeout_peer_next;
  //}}}
  // routing-table {{{
  RoutingTable *root;
  //}}}
  // timeout {{{
  time_t timeout_next;
  Node *timeout_node;
  //}}}
  // recycle contact list {{{
  sp::list<Node> contact_list;
  sp::list<dht::Contact> value_list;
  // }}}
  // {{{
  std::uint16_t sequence;
  time_t last_activity;
  std::uint32_t total_nodes;
  // }}}
  // {{{
  // }}}

  DHT();
};

/*dht::MessageContext*/
struct MessageContext {
  const char *query;

  DHT &dht;

  bencode::d::Decoder &in;
  sp::Buffer &out;

  const krpc::Transaction &transaction;
  Contact remote;
  const time_t now;
  MessageContext(DHT &, const krpc::ParseContext &, sp::Buffer &, Contact,
                 time_t) noexcept;
};
} // namespace dht

#endif
