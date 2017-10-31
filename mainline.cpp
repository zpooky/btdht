#include "mainline.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

namespace dht {

/*KeyValue*/
KeyValue::KeyValue()
    : next(nullptr)
    , peers(nullptr)
    , id() {
}

/*Peer*/
Peer::Peer()
    : ip(0)
    , port()
    , next(nullptr) {
}

static KeyValue *
find_kv(KeyValue *current, const infohash &id) noexcept {
start:
  if (current) {
    if (std::memcmp(id, current->id, sizeof(id)) == 0) {
      return current;
    }
    current = current->next;
    goto start;
  }
  return current;
}

const Peer *
lookup(DHT &dht, const infohash &id) noexcept {
  KeyValue *const needle = find_kv(dht.kv, id);
  if (needle) {
    return needle->peers;
  }
  return nullptr;
}

/*Contact*/
Contact::Contact()
    : last_activity()
    , id()
    , peer()
    , outstanding_ping(false)
    , next(nullptr) {
}

/*Bucket*/
Bucket::Bucket()
    : contacts() {
}

Bucket::~Bucket() {
}

/*RoutingTable*/
RoutingTable::RoutingTable(RoutingTable *h, RoutingTable *l)
    : type(NodeType::NODE) {
  node.higher = h;
  node.lower = l;
}

RoutingTable::RoutingTable()
    : bucket()
    , type(NodeType::LEAF) {
}

RoutingTable::~RoutingTable() {
  if (type == NodeType::LEAF) {
    bucket.~Bucket();
  } else {
    // TODO reclaim
  }
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
bit(const Key &key, std::size_t bitIdx) noexcept {
  std::size_t byte = bitIdx / 8;
  std::size_t bit = bitIdx % 8;
  return key[byte] & bit;
}

static RoutingTable *
find_closest(DHT &dht, const Key &key, bool &inTree,
             std::size_t &bitIdx) noexcept {
  RoutingTable *root = dht.root;
  bitIdx = 0;
start:
  if (root->type == NodeType::NODE) {
    bool high = bit(key, bitIdx);
    // inTree true if share same prefix
    inTree &= bit(dht.id, bitIdx) == high;

    if (high) {
      root = root->node.higher;
    } else {
      root = root->node.lower;
    }

    ++bitIdx;
    goto start;
  }
  return root;
}

template <typename T>
static T *
alloc(DHT &) {
  // raw alloc stash
  void *result = malloc(sizeof(T));
  return new (result) T;
}

static bool
insert(Bucket &bucket, const Contact &c) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Contact &contact = bucket.contacts[i];
    if (!contact) {
      contact = c;
      return true;
    }
  }
  return false;
}

static void
split(DHT &dht, RoutingTable *parent, std::size_t bitIdx) {
  auto higher = alloc<RoutingTable>(dht);
  auto lower = alloc<RoutingTable>(dht);

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Contact &contact = parent->bucket.contacts[i];
    if (contact) {
      bool high = bit(contact.id, bitIdx);
      if (high) {
        assert(insert(higher->bucket, contact));
      } else {
        assert(insert(lower->bucket, contact));
      }
    }
  } // for

  parent->~RoutingTable();
  new (parent) RoutingTable(higher, lower);
}

static bool
add(DHT &dht, const Contact &c) noexcept {
start:
  bool inTree = false;
  std::size_t bitIdx = 0;
  RoutingTable *leaf = find_closest(dht, c.id, inTree, bitIdx);
  assert(leaf);
  Bucket &bucket = leaf->bucket;
  if (!insert(bucket, c)) {
    if (inTree) {
      split(dht, leaf, bitIdx);
      goto start;
    }
    return false;
  }
  return true;
}

static bool
merge_children(RoutingTable *parent) noexcept {
  // TODO
}
static RoutingTable *
find_parent(DHT &dht, const NodeId &) noexcept {
  // TODO
  return nullptr;
}

static bool
uncontact(Contact &c) noexcept {
  // TODO
}

static std::size_t
remove(Bucket &bucket, const Contact &search) noexcept {
  std::size_t result = 0;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Contact &c = bucket.contacts[i];
    assert(c.next == nullptr);
    if (c) {
      if (std::memcmp(c.id, search.id, sizeof(c.id)) == 0) {
        uncontact(c);
      }
      ++result;
    }
  }

  return result;
}

static bool
remove(DHT &dht, Contact &c) noexcept {
  // start:
  RoutingTable *parent = find_parent(dht, c.id);
  assert(parent->type == NodeType::NODE);

  RoutingTable *lower = parent->node.lower;
  RoutingTable *higher = parent->node.higher;

  std::size_t cnt = 0;
  if (lower->type == NodeType::LEAF) {
    cnt += remove(lower->bucket, c);
  }
  if (higher->type == NodeType::LEAF) {
    cnt += remove(higher->bucket, c);
  }
  if (cnt <= Bucket::K) {
    if (lower->type == NodeType::LEAF && higher->type == NodeType::LEAF) {
      merge_children(parent);
      // goto start;
      // TODO recurse
    }
  }
} // namespace dht

static Contact *
contacts_older(RoutingTable *root, const Timestamp &age) noexcept {
  if (root->type == NodeType::NODE) {
    Contact *result = contacts_older(root->node.lower, age);
    Contact *const higher = contacts_older(root->node.higher, age);
    if (result) {
      result->next = higher;
    } else {
      result = higher;
    }
    return result;
  }
  Contact *result = nullptr;
  Bucket &b = root->bucket;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Contact &c = b.contacts[i];
    if (c) {
      assert(c.next == nullptr);
      if (c.last_activity < age) {
        c.next = result;
        result = &c;
      }
    }
  }
  return result;
}
/*DHT*/
DHT::DHT()
    : id()
    , kv(nullptr)
    , root(nullptr) {
}

} // namespace dht
