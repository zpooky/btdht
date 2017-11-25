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
KeyValue::KeyValue(const Infohash &pid, KeyValue *nxt)
    : next(nxt)
    , peers(nullptr)
    , id() {
  std::memcpy(id.id, pid.id, sizeof(id.id));
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

static Node *
insert(DHT &dht, Bucket &bucket, const Node &c, bool eager,
       time_t now) noexcept {
  auto is_good = [now](Node &contact) {
    Config config;
    // XXX configurable non arbitrary limit?
    if (contact.ping_outstanding > 2) {
      if (contact.response_activity + config.refresh_interval < now) {
        if (contact.request_activity + config.refresh_interval < now) {
          return false;
        }
      }
    }
    return true;
  };

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (!contact) {
      contact = c;
      return &contact;
    }
  }
  if (eager) {
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = bucket.contacts[i];
      if (!is_good(contact)) {
        timeout::unlink(dht, &contact);
        contact = c;
        timeout::append_all(dht, &contact);
        return &contact;
      }
    }
  }
  return nullptr;
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
      // TODO fixup the timout_next&priv chain with the new adresses +tail&head
      bool high = bit(contact.id, idx);
      if (high) {
        assert(insert(dht, higher->bucket, contact, false, time_t(0)));
      } else {
        assert(insert(dht, lower->bucket, contact, false, time_t(0)));
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

// static std::size_t
// remove(Bucket &bucket, const Node &search) noexcept {
//   std::size_t result = 0;
//   for (std::size_t i = 0; i < Bucket::K; ++i) {
//     Node &c = bucket.contacts[i];
//     assert(c.timeout_next == nullptr);
//     if (c) {
//       if (std::memcmp(c.id.id, search.id.id, sizeof(c.id)) == 0) {
//         uncontact(c);
//       }
//       ++result;
//     }
//   }
//
//   return result;
// }
//
// static bool
// remove(DHT &dht, Node &c) noexcept {
//   // start:
//   RoutingTable *parent = find_parent(dht, c.id);
//   assert(parent->type == NodeType::NODE);
//
//   RoutingTable *lower = parent->node.lower;
//   RoutingTable *higher = parent->node.higher;
//
//   std::size_t cnt = 0;
//   if (lower->type == NodeType::LEAF) {
//     cnt += remove(lower->bucket, c);
//   }
//   if (higher->type == NodeType::LEAF) {
//     cnt += remove(higher->bucket, c);
//   }
//   if (cnt <= Bucket::K) {
//     if (lower->type == NodeType::LEAF && higher->type == NodeType::LEAF) {
//       merge_children(dht, parent);
//       // goto start;
//       // TODO recurse
//     }
//   }
//   return true;
// } // namespace dht

// static Node *
// contacts_older(RoutingTable *root, time_t age) noexcept {
//   if (root->type == NodeType::NODE) {
//     Node *result = contacts_older(root->node.lower, age);
//     Node *const higher = contacts_older(root->node.higher, age);
//     if (result) {
//       result->timeout_next = higher;
//     } else {
//       result = higher;
//     }
//     return result;
//   }
//
//   Node *result = nullptr;
//   Bucket &b = root->bucket;
//   for (std::size_t i = 0; i < Bucket::K; ++i) {
//     Node &contact = b.contacts[i];
//     if (contact) {
//       assert(contact.timeout_next == nullptr);
//       if (contact.activity < age) {
//         contact.timeout_next = result;
//         result = &contact;
//       }
//     }
//   }
//   return result;
// }

/*TokenPair*/
TokenPair::TokenPair()
    : ip()
    , token()
    , created() {
}

TokenPair::operator bool() const noexcept {
  return ip != Ip(0);
}

/*DHT*/
DHT::DHT()
    // self {{{
    : id()
    //}}}
    // peer-lookup db {{{
    , lookup_table(nullptr)
    , tokens()
    //}}}
    // routing-table {{{
    , root(nullptr)
    //}}}
    // timeout{{{
    , timeout_next(0)
    , timeout_head(nullptr)
    , timeout_tail(nullptr)
    //}}}
    // recycle contact list {{{
    , contact_list()
    , value_list()
    // }}}
    // {{{
    , sequence(0)
//}}}
{
}

static std::size_t
index(Ip ip) noexcept {
  return ip % DHT::token_table;
}

void
mintToken(DHT &dht, Ip ip, const Token &token) noexcept {
  // TODO
}

bool
is_blacklisted(DHT &dht, const dht::Contact &) noexcept {
  // XXX
  return false;
}

//============================================================
bool
init(dht::DHT &dht) noexcept {
  if (!sp::init(dht.contact_list, 16)) {
    return false;
  }
  if (!sp::init(dht.value_list, 64)) {
    return false;
  }
  return true;
}

static void
find_closest_nodes(DHT &dht, const Key &search, Node *(&result)[Bucket::K],
                   std::size_t number) noexcept {
  RoutingTable *root = dht.root;
  std::size_t idx = 0;
  // TODO circular buff insert for all leafs?
  // TODO
start:
  if (root->type == NodeType::NODE) {
    if (bit(search, idx) == true) {
      root = root->node.higher;
    } else {
      root = root->node.lower;
    }

    ++idx;
    goto start;
  }
  // return root;
  // return dht.contact_list;
  return;
} // dht::find_closest_internal()

/*public*/
void
find_closest(DHT &dht, const NodeId &id, Node *(&result)[Bucket::K],
             std::size_t number) noexcept {
  return find_closest_nodes(dht, id.id, result, number);
} // dht::find_closes()

void
find_closest(DHT &dht, const Infohash &id, Node *(&result)[Bucket::K],
             std::size_t number) noexcept {
  return find_closest_nodes(dht, id.id, result, number);
} // dht::find_closest()

bool
valid(DHT &dht, const krpc::Transaction &) noexcept {
  // TODO list of active transaction
  return true;
} // dht::valid()

Node *
find_contact(DHT &dht, const NodeId &id) noexcept {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, id, inTree, idx);
  assert(leaf);
  // TODO how to ensure leaf is a bucket?

  return find(leaf->bucket, id);
} // dht::find_contact()

Node *
insert(DHT &dht, const Node &contact, time_t now) noexcept {
start:
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, contact.id, inTree, idx);
  assert(leaf);

  Bucket &bucket = leaf->bucket;
  // when we are intree meaning we can add another bucket we do not necessarily
  // need to evict a node that might be late responding to pings

  Node *result = insert(dht, bucket, contact, !inTree, now);
  if (!result) {
    if (inTree) {
      split(dht, leaf, idx);
      // XXX make better
      goto start;
    }
  }
  return result;
} // dht::insert()

} // namespace dht

namespace timeout {
void
unlink(dht::DHT &ctx, dht::Node *const contact) noexcept {
  dht::Node *priv = contact->timeout_priv;
  dht::Node *next = contact->timeout_next;
  if (priv)
    priv->timeout_next = next;

  if (next)
    next->timeout_priv = priv;

  if (ctx.timeout_tail == contact)
    ctx.timeout_tail = priv;

  if (ctx.timeout_head == contact)
    ctx.timeout_head = next;
} // timeout::unlink()

static dht::Node *
last(dht::Node *node) noexcept {
Lstart:
  if (node) {
    if (node->timeout_next) {
      node = node->timeout_next;
      goto Lstart;
    }
  }
  return node;
} // timeout::last()

void
append_all(dht::DHT &ctx, dht::Node *node) noexcept {
  if (ctx.timeout_tail) {
    ctx.timeout_tail->timeout_next = node;
    node->timeout_priv = ctx.timeout_tail;
  }

  dht::Node *l = last(node);
  ctx.timeout_tail = l;
  l->timeout_priv = ctx.timeout_tail;
} // timeout::append_all()
} // namespace timeout

namespace lookup {
dht::KeyValue *
lookup(dht::DHT &dht, const dht::Infohash &id, time_t now) noexcept {
  auto find_haystack = [](dht::KeyValue *current, const dht::Infohash &id) {
    // XXX tree?
  start:
    if (current) {
      if (std::memcmp(id.id, current->id.id, sizeof(id)) == 0) {
        return current;
      }
      current = current->next;
      goto start;
    }

    return current;
  };

  auto is_expired = [&dht, now](auto &peer) {
    time_t last_lookup_refresh = dht.lookup_refresh;
    time_t peer_activity = peer.activity;

    // Determine to age if end of life is higher than now and make sure that
    // we have an internet connection by checking that we have received any
    // updates at all
    dht::Config config;
    time_t peer_eol = peer_activity + config.peer_age_refresh;
    if (peer_eol < now) {
      if (last_lookup_refresh > peer_eol) {
        return true;
      }
    }
    return false;
  };

  auto reclaim = [&dht](dht::Peer *peer) { //
    // XXX pool
    delete peer;
  };

  auto reclaim_table = [&dht](dht::KeyValue *kv) { //
    // XXX pool
    delete kv;
  };

  dht::KeyValue *const needle = find_haystack(dht.lookup_table, id);
  if (needle) {
    dht::Peer dummy;
    dht::Peer *it = dummy.next = needle->peers;

    dht::Peer *previous = &dummy;
  Lloop:
    if (it) {
      dht::Peer *const next = it->next;
      if (is_expired(*it)) {
        reclaim(it);
        previous->next = next;
      }
      previous = it;

      it = next;
      goto Lloop;
    }
    needle->peers = dummy.next;
    if (needle->peers) {
      return needle;
    }
    reclaim_table(needle);
  }
  return nullptr;
}

bool
insert(dht::DHT &dht, const dht::Infohash &id, const dht::Contact &contact,
       time_t now) noexcept {
  auto create = [&dht](const dht::Infohash &id) -> dht::KeyValue * {
    auto result = new dht::KeyValue(id, dht.lookup_table);
    if (result) {
      dht.lookup_table = result;
    }
    return result;
  };

  auto add = [&dht, now](dht::KeyValue &s, const dht::Contact &c) {
    auto p = new dht::Peer(c, now, s.peers);
    if (p) {
      s.peers = p;
      return true;
    }
    return false;
  };

  dht::KeyValue *needle = lookup(dht, id, now);
  if (!needle) {
    needle = create(id);
  }
  if (needle) {

    if (add(*needle, contact)) {
      return true;
    }
    if (needle->peers == nullptr) {
      // TODO if add false and create needle reclaim needle
    }
  }

  return false;
}

bool
valid(dht::DHT &, const dht::Token &) noexcept {
  // TODO
  return true;
}

} // namespace lookup
