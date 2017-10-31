#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"

// #Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
/*mainline dht*/

using Key = sp::byte[20];
using infohash = Key;

//- Each node has a globally unique identifier known as the nodeID
//- Node IDs are chosen at random
using NodeId = Key;

struct Peer {
  std::uint32_t ip;
  std::uint16_t port;
  // {
  Peer *next;
  // }
  Peer();
};

using Token = int;               // TODO
using Timestamp = std::uint32_t; // TODO

/*lookup*/
struct KeyValue {
  KeyValue *next;
  Peer *peers;
  infohash id;
  //
  KeyValue();
};

/*Contact*/
// 15 min refresh
struct Contact {
  Timestamp last_activity;
  NodeId id;
  Peer peer;
  bool outstanding_ping;

  // {
  Contact *next;
  // }

  Contact();

  explicit operator bool() const noexcept {
    // TODO
    return true;
  }
};

/*Bucket*/
struct Bucket {
  static constexpr std::size_t K = 8;
  Contact contacts[K];

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
  const NodeId id;
  KeyValue *kv;
  RoutingTable *root;

  DHT();
};

const Peer *
lookup(const DHT &, const infohash &) noexcept;

} // namespace dht
#endif
