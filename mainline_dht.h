#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "kadmelia.h"

// #Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
/*mainline dht*/
using infohash = kadmelia::Key;

//- Each node has a globally unique identifier known as the nodeID
//- Node IDs are chosen at random
using NodeId = kadmelia::Key;

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
  bool outstanding_ping;
  Peer peer;
  Contact();
};

/*Bucket*/
struct Bucket {
  Contact contacts[8];
  Bucket *higher;
  Bucket *lower;
  Bucket();
  ~Bucket();
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

// When a node wants to find peers for a torrent, it uses the distance metric to
// compare the infohash of the torrent with the IDs of the nodes in its own
// routing table.
// It then contacts the nodes it knows about with IDs closest to
// the infohash and asks them for the contact information of peers currently
// downloading the torrent.
// If a contacted node knows about peers for the
// torrent, the peer contact information is returned with the response.
// Otherwise, the contacted node must respond with the contact information of
// the nodes in its routing table that are closest to the infohash of the
// torrent.
void
get_peers(const infohash &);
} // namespace dht
#endif
