#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"

// #Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
/*mainline dht*/

using Sha1 = sp::byte[20];
using Key = Sha1;
using infohash = Key;

//- Each node has a globally unique identifier known as the nodeID
//- Node IDs are chosen at random
using NodeId = Key;

struct Peer {
  std::uint32_t ip;
  std::uint16_t port;
};

using Token = int;               // TODO
using Timestamp = std::uint32_t; // TODO

/*Contact*/
// 15 min refresh
struct Contact {
  Timestamp last_activity;
  Peer peer;
  bool outstanding_ping;

  Contact();
};

/*Bucket*/
struct Bucket {
  Contact contacts[8];

  Bucket();
  ~Bucket();
};

/*Tree*/
enum class NodeType { NODE, LEAF };
struct Tree {
  union {
    struct {
      Tree *higher;
      Tree *lower;
      Key middle;
    } node;
    Bucket bucket;
  };
  NodeType type;

  Tree(Tree *h, const Key &middle, Tree *l);
  Tree();
};

void
split(Bucket *);

/*RoutingTable*/
// Nodes maintain a routing table containing the contact information for a
// small number of other nodes.
// The routing table becomes more detailed as IDs get closer to the node's own
// ID. Nodes know about many other nodes in the DHT that have IDs that are
// "close" to their own but have only a handful of contacts with IDs that are
// very far away from their own.
struct RoutingTable {
  Bucket *base;

  RoutingTable();
  ~RoutingTable();
};

} // namespace dht
#endif
