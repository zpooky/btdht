#include "mainline.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

namespace dht {

void
randomize(NodeId &id) noexcept {
  sp::byte *it = id;
  std::size_t remaining = sizeof(id);

  while (remaining > 0) {
    const int r = rand();
    std::size_t length = std::min(sizeof(int), remaining);

    std::memcpy(it, &r, length);
    remaining -= length;
    it += length;
  }
}

/*KeyValue*/
KeyValue::KeyValue()
    : next(nullptr)
    , peers(nullptr)
    , id() {
}

/*Peer*/
Peer::Peer()
    : ip(0)
    , port(0)
    , next(nullptr) {
}

static KeyValue *
find_kv(KeyValue *current, const infohash &id) noexcept {
  // TODO tree?
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

Contact::operator bool() const noexcept {
  return peer.ip == 0;
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
    // dealloc(dht,lower);
    // dealloc(dht,higher);
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
bit(const Key &key, std::size_t idx) noexcept {
  const std::size_t byte = idx / 8;
  const std::uint8_t bit = idx % 8;
  const std::uint8_t bitMask = std::uint8_t(0b1000'0000) >> bit;
  return key[byte] & bitMask;
}

static RoutingTable *
find_closest(DHT &dht, const Key &key, bool &inTree,
             std::size_t &idx) noexcept {
  RoutingTable *root = dht.root;
  idx = 0;
start:
  if (root->type == NodeType::NODE) {
    bool high = bit(key, idx);
    // inTree true if share same prefix
    inTree &= bit(dht.id, idx) == high;

    if (high) {
      root = root->node.higher;
    } else {
      root = root->node.lower;
    }

    ++idx;
    goto start;
  }
  return root;
}

template <typename T>
static void
dealloc(DHT &, T *reclaim) {
  reclaim->~T();
  free(reclaim);
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
split(DHT &dht, RoutingTable *parent, std::size_t idx) {
  auto higher = alloc<RoutingTable>(dht);
  auto lower = alloc<RoutingTable>(dht);

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Contact &contact = parent->bucket.contacts[i];
    if (contact) {
      bool high = bit(contact.id, idx);
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
  std::size_t idx = 0;
  RoutingTable *leaf = find_closest(dht, c.id, inTree, idx);
  assert(leaf);
  Bucket &bucket = leaf->bucket;
  if (!insert(bucket, c)) {
    if (inTree) {
      split(dht, leaf, idx);
      goto start;
    }
    return false;
  }
  return true;
}

static bool
copy(const Contact &from, Bucket &to) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Contact &c = to.contacts[i];
    if (!c) {
      c = from;
      return true;
    }
  }
  return false;
}

static bool
copy(const Bucket &from, Bucket &to) noexcept {
  for (std::size_t f = 0; f < Bucket::K; ++f) {
    const Contact &c = from.contacts[f];
    if (c) {
      if (!copy(c, to)) {
        return false;
      }
    }
  }
  return true;
}

static bool
merge_children(DHT &dht, RoutingTable *parent) noexcept {
  RoutingTable *lower = parent->node.lower;
  assert(lower->type == NodeType::LEAF);
  RoutingTable *higher = parent->node.higher;
  assert(higher->type == NodeType::LEAF);

  parent->~RoutingTable();
  parent = new (parent) RoutingTable;

  copy(lower->bucket, parent->bucket);
  copy(higher->bucket, parent->bucket);

  dealloc(dht, lower);
  dealloc(dht, higher);
}

static RoutingTable *
find_parent(DHT &dht, const NodeId &) noexcept {
  // TODO
  return nullptr;
}

static bool
uncontact(Contact &c) noexcept {
  c = Contact();
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
      merge_children(dht, parent);
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
