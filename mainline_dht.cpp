#include "mainline_dht.h"
#include <cstring>

namespace dht {

/*Contact*/
Contact::Contact()
    : last_activity()
    , peer()
    , outstanding_ping(false) {
}

/*Bucket*/
Bucket::Bucket()
    : contacts() {
}
Bucket::~Bucket() {
}

/*Tree*/
Tree::Tree(Tree *h, const Key &middle, Tree *l)
    : type(NodeType::NODE) {
  node.higher = nullptr;
  std::memcpy(node.middle, middle, sizeof(Key));
  node.lower = nullptr;
}

Tree::Tree()
    : bucket()
    , type(NodeType::LEAF) {
}

void
split(Bucket *) {
}

/*RoutingTable*/
RoutingTable::RoutingTable()
    : base(nullptr) {
}
RoutingTable::~RoutingTable() {
}

/**/
static void
distance(const Key &a, const Key &b, Key &result) {
  // distance(A,B) = |A xor B| Smaller values are closer.
  for (std::size_t i = 0; i < sizeof(Key); ++i) {
    result[i] = a[i] ^ b[i];
  }
}
static bool
lt(const Key &a, const Key &b) {
  return false;
}
static Bucket *
find_closest(Tree *root, const Key &key) noexcept {
start:
  if (root->type == NodeType::NODE) {
    if (lt(key, root->node.middle)) {
      root = root->node.lower;
    } else {
      root = root->node.higher;
    }
    goto start;
  }
  return &root->bucket;
}

static void
contact_older(const Bucket &,
              const Timestamp &age /*,Resulting Contacts*/) noexcept {
}

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
get_peers(RoutingTable &, const infohash &key) {
}

} // namespace dht
