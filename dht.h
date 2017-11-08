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

  DHT();
};
/**/
bool
update_activity(DHT &, const NodeId &, time_t, bool ping) noexcept;

bool
add(DHT &, const Node &) noexcept;

/**/
const Peer *
lookup(const DHT &, const Infohash &) noexcept;

} // namespace dht
#endif
