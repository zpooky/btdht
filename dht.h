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

/*DHT*/
struct DHT {
  NodeId id;
  KeyValue *kv;
  RoutingTable *root;
  // timeout {{{
  time_t timeout_next;
  Node *timeout_head;
  Node *timeout_tail;
  //}}}
  // recycle {{{
  sp::list<Node> contact_list;
  sp::list<Peer> value_list;
  // }}}

  DHT();
};
/**/
sp::list<Node> &
find_closest(DHT &, const NodeId &, std::size_t) noexcept;

sp::list<Node> &
find_closest(DHT &, const Infohash &, std::size_t) noexcept;

bool
valid(DHT &, const krpc::Transaction &) noexcept;

Node *
find_contact(DHT &, const NodeId &) noexcept;

Node *
add(DHT &, const Node &) noexcept;

} // namespace dht

namespace timeout {
void
unlink(dht::DHT &, dht::Node *) noexcept;

void
append(dht::DHT &, dht::Node *) noexcept;
} // namespace timeout

namespace lookup {
/**/
const dht::Peer *
get(dht::DHT &, const dht::Infohash &) noexcept;

void
insert(dht::DHT &, const dht::Infohash &, const dht::Peer &) noexcept;
} // namespace lookup

#endif
