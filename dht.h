#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"

// #Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
void
randomize(NodeId &) noexcept;

/*lookup*/
struct KeyValue {
  KeyValue *next;
  Peer *peers;
  time_t activity;
  Infohash id;
  //
  KeyValue();
};

/*Bucket*/
struct Bucket {
  static constexpr std::size_t K = 8;
  Node contacts[K];

  Bucket();
  ~Bucket();
};

/*RoutingTable*/
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

struct TokenPair {
  Ip ip;
  Token token;
  time_t created;

  operator bool() const noexcept;

  TokenPair();
};

/*DHT*/
struct DHT {
  static const std::size_t tables = 64;
  // self {{{
  NodeId id;
  //}}}
  // peer-lookup db {{{
  KeyValue *lookup_table;
  Token tokens[tables];
  time_t lookup_refresh;
  //}}}
  // routing-table {{{
  RoutingTable *root;
  //}}}
  // timeout {{{
  time_t timeout_next;
  Node *timeout_head;
  Node *timeout_tail;
  //}}}
  // recycle contact list {{{
  sp::list<Node> contact_list;
  sp::list<dht::Contact> value_list;
  // }}}
  // {{{
  std::uint16_t sequence;
  // }}}

  DHT();
};

bool
init(dht::DHT &) noexcept;

void
mintToken(DHT &, Ip, const Token &) noexcept;

bool
is_blacklisted(DHT &dht, const dht::Contact &) noexcept;

/**/
void
find_closest(DHT &, const NodeId &, Node *(&)[Bucket::K], std::size_t) noexcept;

void
find_closest(DHT &, const Infohash &, Node *(&)[Bucket::K],
             std::size_t) noexcept;

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
const dht::Peer *
lookup(dht::DHT &, const dht::Infohash &, time_t) noexcept;

void
insert(dht::DHT &, const dht::Infohash &, const dht::Contact &) noexcept;

bool
valid(dht::DHT &, const dht::Token &) noexcept;
} // namespace lookup

#endif
