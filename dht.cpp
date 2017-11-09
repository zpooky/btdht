#include "dht.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

namespace dht {

void
randomize(NodeId &id) noexcept {
  sp::byte *it = id.id;
  std::size_t remaining = sizeof(id.id);

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

static KeyValue *
find_kv(KeyValue *current, const Infohash &id) noexcept {
  // TODO tree?
start:
  if (current) {
    if (std::memcmp(id.id, current->id.id, sizeof(id)) == 0) {
      return current;
    }
    current = current->next;
    goto start;
  }
  return current;
}

const Peer *
lookup(DHT &dht, const Infohash &id) noexcept {
  KeyValue *const needle = find_kv(dht.kv, id);
  if (needle) {
    return needle->peers;
  }
  return nullptr;
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

static bool
bit(const NodeId &key, std::size_t idx) noexcept {
  return bit(key.id, idx);
}

static RoutingTable *
find_closest(DHT &dht, const NodeId &key, bool &inTree,
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
insert(Bucket &bucket, const Node &c) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (!contact) {
      contact = c;
      return true;
    }
  }
  return false;
}

static Node *
find(Bucket &bucket, const NodeId &id) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (contact) {
      if (std::memcmp(contact.id.id, id.id, sizeof(id)) == 0) {
        return &contact;
      }
    }
  }
  return nullptr;
}

static void
split(DHT &dht, RoutingTable *parent, std::size_t idx) {
  auto higher = alloc<RoutingTable>(dht);
  auto lower = alloc<RoutingTable>(dht);

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = parent->bucket.contacts[i];
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
copy(const Node &from, Bucket &to) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &c = to.contacts[i];
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
    const Node &c = from.contacts[f];
    if (c) {
      if (!copy(c, to)) {
        return false;
      }
    }
  }
  return true;
}

static void
merge_children(DHT &dht, RoutingTable *parent) noexcept {
  RoutingTable *lower = parent->node.lower;
  assert(lower->type == NodeType::LEAF);
  RoutingTable *higher = parent->node.higher;
  assert(higher->type == NodeType::LEAF);

  parent->~RoutingTable();
  parent = new (parent) RoutingTable;

  assert(copy(lower->bucket, parent->bucket));
  assert(copy(higher->bucket, parent->bucket));

  dealloc(dht, lower);
  dealloc(dht, higher);
}

static RoutingTable *
find_parent(DHT &, const NodeId &) noexcept {
  // TODO
  return nullptr;
}

static void
uncontact(Node &c) noexcept {
  c = Node();
}

static std::size_t
remove(Bucket &bucket, const Node &search) noexcept {
  std::size_t result = 0;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &c = bucket.contacts[i];
    assert(c.next == nullptr);
    if (c) {
      if (std::memcmp(c.id.id, search.id.id, sizeof(c.id)) == 0) {
        uncontact(c);
      }
      ++result;
    }
  }

  return result;
}

static bool
remove(DHT &dht, Node &c) noexcept {
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
  return true;
} // namespace dht

static Node *
contacts_older(RoutingTable *root, const time_t &age) noexcept {
  if (root->type == NodeType::NODE) {
    Node *result = contacts_older(root->node.lower, age);
    Node *const higher = contacts_older(root->node.higher, age);
    if (result) {
      result->next = higher;
    } else {
      result = higher;
    }
    return result;
  }
  Node *result = nullptr;
  Bucket &b = root->bucket;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = b.contacts[i];
    if (contact) {
      assert(contact.next == nullptr);
      if (contact.activity < age) {
        contact.next = result;
        result = &contact;
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

/*public*/
bool
update_activity(DHT &dht, const NodeId &id, time_t t, bool ping) noexcept {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, id, inTree, idx);
  assert(leaf);
  // TODO how to ensure leaf is a bucket?
  Node *contact = find(leaf->bucket, id);
  if (contact) {
    contact->activity = std::max(t, contact->activity);
    if (ping) {
      contact->ping_await = false;
    }
    return true;
  }
  return true;
}

bool
add(DHT &dht, const Node &contact) noexcept {
start:
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, contact.id, inTree, idx);
  assert(leaf);
  Bucket &bucket = leaf->bucket;
  if (!insert(bucket, contact)) {
    if (inTree) {
      split(dht, leaf, idx);
      goto start;
    }
    return false;
  }
  return true;
}

} // namespace dht
