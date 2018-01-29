#include "Log.h"
#include "dht.h"
#include "timeout.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>
#include <util/CircularBuffer.h>

namespace dht {

static bool
is_cycle(DHT &dht) noexcept {
  Peer *const head = dht.timeout_peer;
  if (head) {
    Peer *it = head;
  Lit:
    if (head) {
      Peer *next = it->timeout_next;
      assert(it == next->timeout_priv);

      it = next;
      if (it != head) {
        goto Lit;
      }
    } else {
      assert(head);
    }
  }
  return true;
}

template <std::size_t SIZE>
static bool
randomize(sp::byte (&buffer)[SIZE]) noexcept {
  sp::byte *it = buffer;
  std::size_t remaining = SIZE;

  while (remaining > 0) {
    const int r = rand();
    std::size_t length = std::min(sizeof(r), remaining);

    std::memcpy(it, &r, length);
    remaining -= length;
    it += length;
  }
  return true;
}

static bool
randomize(const Contact &, NodeId &id) noexcept {
  // TODO
  return randomize(id.id);
}

void
randomize(NodeId &id) noexcept {
  randomize(id.id);
}

static RoutingTable *
find_closest(DHT &dht, const NodeId &search, bool &in_tree,
             std::size_t &idx) noexcept {
  in_tree = true;
  RoutingTable *root = dht.root;
  idx = 0;

Lstart:
  if (root) {
    bool high = bit(search, idx);
    bool ref_high = bit(dht.id, idx);
    // in_tree is true if search so far share the same prefix with dht.id
    in_tree &= ref_high == high;

    if (in_tree) {
      if (root->in_tree) {
        root = root->in_tree;
      } else {
        return root;
      }
    } else {
      return root;
    }

    ++idx;
    goto Lstart;
  }
  return root;
}

template <typename T>
static void
dealloc(DHT &, T *reclaim) {
  // reclaim->~T();
  // free(reclaim);
  delete reclaim;
}

template <typename T>
static T *
alloc(DHT &) {
  // raw alloc stash
  // void *result = malloc(sizeof(T));
  return new T;
}

static void
reset(DHT &dht, Node &contact) noexcept {
  if (!contact.good) {
    assert(dht.bad_nodes > 0);
    dht.bad_nodes--;
    contact.good = true;
  }
  contact = Node();
}

static Node *
do_insert(DHT &dht, Bucket &bucket, const Node &c, bool eager,
          bool &replaced) noexcept {
  replaced = false;

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (!contact) {
      contact = c;
      assert(contact);

      // timeout::append_all(dht, &contact);
      return &contact;
    }
  }

  if (eager) {
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = bucket.contacts[i];
      if (!is_good(dht, contact)) {
        timeout::unlink(dht, &contact);
        reset(dht, contact);
        assert(!contact);

        contact = c;
        assert(contact);

        // timeout::append_all(dht, &contact);

        replaced = true;
        return &contact;
      }
    }
  }

  return nullptr;
}

static Node *
find(Bucket &bucket, const Key &id) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (contact) {
      if (contact.id == id) {
        return &contact;
      }
    }
  }
  return nullptr;
}

static Node *
find(Bucket &bucket, const NodeId &id) noexcept {
  return find(bucket, id.id);
}

static bool
split(DHT &dht, RoutingTable *parent, std::size_t idx) noexcept {
  assert(parent->in_tree == nullptr);

  auto in_tree = alloc<RoutingTable>(dht);
  if (!in_tree) {
    return false;
  }

  auto should_move = [&dht, idx](const Node &n) { //
    const bool current_high = bit(n.id, idx);
    const bool in_tree_high = bit(dht.id, idx);
    return current_high == in_tree_high;
  };

  Bucket &bucket = parent->bucket;
  std::size_t moved = 0;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (contact) {
      if (should_move(contact)) {
        Node *const priv = contact.timeout_priv;
        Node *const next = contact.timeout_next;

        auto relink = [priv, next](dht::Node *c) {
          c->timeout_priv = priv;
          c->timeout_next = next;
          if (priv)
            priv->timeout_next = c;
          if (next)
            next->timeout_priv = c;
        };

        assert(is_cycle(dht));
        timeout::unlink(dht, &contact);
        assert(!contact.timeout_next);
        assert(!contact.timeout_priv);

        const bool eager = false;
        bool replaced /*OUT*/ = false;
        auto *nc = do_insert(dht, in_tree->bucket, contact, eager, replaced);
        assert(!replaced);
        assert(nc);

        if (nc) {
          relink(nc);
          // reset
        }
        contact = Node();
        assert(!contact);

        ++moved;
      }
    }
  } // for
  if (moved == Bucket::K) {
    // printf("moved all from [%p] to[%p]\n", parent, in_tree);
  }

  parent->in_tree = in_tree;

  // TODO log::routing::split(dht, *higher, *lower);
  return true;
}

// static bool
// copy(const Node &from, Bucket &to) noexcept {
//   for (std::size_t i = 0; i < Bucket::K; ++i) {
//     Node &c = to.contacts[i];
//     if (!c) {
//       c = from;
//       return true;
//     }
//   }
//   return false;
// }

// static bool
// copy(const Bucket &from, Bucket &to) noexcept {
//   for (std::size_t f = 0; f < Bucket::K; ++f) {
//     const Node &c = from.contacts[f];
//     if (c) {
//       if (!copy(c, to)) {
//         return false;
//       }
//     }
//   }
//   return true;
// }

// static void
// merge_children(DHT &dht, RoutingTable *parent) noexcept {
//   RoutingTable *lower = parent->node.lower;
//   assert(lower->type == NodeType::LEAF);
//   RoutingTable *higher = parent->node.higher;
//   assert(higher->type == NodeType::LEAF);
//
//   parent->~RoutingTable();
//   parent = new (parent) RoutingTable;
//
//   assert(copy(lower->bucket, parent->bucket));
//   assert(copy(higher->bucket, parent->bucket));
//
//   dealloc(dht, lower);
//   dealloc(dht, higher);
// }

// static RoutingTable *
// find_parent(DHT &, const NodeId &) noexcept {
//   // TODO
//   return nullptr;
// }

// static void
// uncontact(Node &c) noexcept {
//   c = Node();
// }

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
struct TokenPair {
  Ipv4 ip;
  Token token;
  time_t created;

  TokenPair();
  operator bool() const noexcept;
};

/*TokenPair*/
TokenPair::TokenPair()
    : ip()
    , token()
    , created() {
}

TokenPair::operator bool() const noexcept {
  return ip != Ipv4(0);
}

// static bool
// bit_compare(const NodeId &id, const Key &cmp, std::size_t length) noexcept {
//   for (std::size_t i = 0; i < length; ++i) {
//     if (bit(id.id, i) != bit(cmp, i)) {
//       return false;
//     }
//   }
//   return true;
// }

static std::size_t
shared_prefix(const NodeId &id, const Key &cmp) noexcept {
  std::size_t i = 0;
  for (; i < NodeId::bits; ++i) {
    if (bit(id.id, i) != bit(cmp, i)) {
      return i;
    }
  }
  return i;
}

static void
multiple_closest_nodes(DHT &dht, const Key &search,
                       Node *(&result)[Bucket::K]) noexcept {
  Bucket *raw[Bucket::K];
  sp::CircularBuffer best(raw);

  RoutingTable *root = dht.root;
  std::size_t idx = 0;
Lstart:
  if (root) {
    sp::push_back(best, &root->bucket);
    if (idx++ < shared_prefix(dht.id, search)) {
      root = root->in_tree;
      goto Lstart;
    }
  }

  std::size_t resIdx = 0;
  auto merge = [&dht, &resIdx, &result](Bucket &b) -> bool { //
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = b.contacts[i];
      if (contact) {
        if (is_good(dht, contact)) {
          result[resIdx++] = &contact;
          if (resIdx == Bucket::K) {
            return true;
          }
        }
      }
    }
    return false;
  };

  {
    Bucket *best_ordered[Bucket::K]{nullptr};
    std::size_t best_idx = 0;

    while (!is_empty(best)) {
      sp::pop_back(best, best_ordered[best_idx++]);
    }

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      if (best_ordered[i]) {
        // while (resIdx < Bucket::K) {

        if (merge(*best_ordered[i])) {
          return;
        }
      }
    }
  }
} // dht::find_closest_nodes()

//============================================================
bool
is_blacklisted(DHT &, const Contact &) noexcept {
  // XXX
  return false;
}

bool
should_mark_bad(DHT &dht, Node &contact) noexcept {
  // TODO
  return !is_good(dht, contact);
}

bool
is_good(const DHT &dht, const Node &contact) noexcept {
  Config config;
  // XXX configurable non arbitrary limit?
  if (contact.ping_outstanding > 2) {

    /*
     * Using dht.last_activty to better handle a general outgate of network
     * connectivity
     */
    time_t resp_timeout = contact.response_activity + config.refresh_interval;
    if (resp_timeout > dht.last_activity) {

      time_t req_activity = contact.request_activity + config.refresh_interval;
      if (req_activity > dht.last_activity) {
        return false;
      }
    }
  }
  return true;
}

bool
init(dht::DHT &dht) noexcept {
  if (!sp::init(dht.recycle_contact_list, 64)) {
    return false;
  }
  if (!sp::init(dht.recycle_value_list, 64)) {
    return false;
  }
  if (!sp::init(dht.bootstrap_contacts, 8)) {
    return false;
  }
  if (!randomize(dht.ip, dht.id)) {
    return false;
  }
  if (!init(dht.client)) {
    return false;
  }
  return true;
}

/*public*/
void
multiple_closest(DHT &dht, const NodeId &id,
                 Node *(&result)[Bucket::K]) noexcept {
  return multiple_closest_nodes(dht, id.id, result);
} // dht::multiple_closest()

void
multiple_closest(DHT &dht, const Infohash &id,
                 Node *(&res)[Bucket::K]) noexcept {
  return multiple_closest_nodes(dht, id.id, res);
} // dht::multiple_closest()

Node *
find_contact(DHT &dht, const NodeId &search) noexcept {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, search, inTree, idx);
  if (leaf) {
    // XXX how to ensure leaf is a bucket?

    return find(leaf->bucket, search);
  }
  return nullptr;
} // dht::find_contact()

Bucket *
bucket_for(DHT &dht, const NodeId &id) noexcept {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, id, inTree, idx);
  return leaf ? &leaf->bucket : nullptr;
}

Node *
insert(DHT &dht, const Node &contact) noexcept {
  auto can_split = [&dht](const Bucket &bucket, std::size_t idx) {
    std::size_t bits[2] = {0};
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      const Node &c = bucket.contacts[i];
      if (c) {
        std::size_t bit_idx = bit(c.id, idx) ? 0 : 1;
        bits[bit_idx]++;
      }
    }

    if (bits[0] > 0 && bits[1] > 0) {
      return true;
    }

    return false;
  };

  if (!is_valid(contact.id)) {
    return nullptr;
  }

Lstart:
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *const leaf = find_closest(dht, contact.id, inTree, idx);
  if (leaf) {
    Bucket &bucket = leaf->bucket;
    {
      /*check if already present*/
      Node *const existing = find(bucket, contact.id);
      if (existing) {
        return existing;
      }
    }
    // when we are intree meaning we can add another bucket we do not
    // necessarily need to evict a node that might be late responding to pings
    bool eager_merge = /*!inTree;*/ false;
    bool /*OUT*/ replaced = false;
    Node *inserted = do_insert(dht, bucket, contact, eager_merge, replaced);

    if (inserted) {
      // printf("- insert\n");
      timeout::append_all(dht, inserted);
      assert(is_cycle(dht));
      assert(inserted->timeout_next);
      assert(inserted->timeout_priv);

      log::routing::insert(dht, *inserted);
      if (!replaced) {
        ++dht.total_nodes;
      }
    } else {
      if (inTree || can_split(bucket, idx)) {
        if (split(dht, leaf, idx)) {
          // XXX make better
          goto Lstart;
        }
        assert(false);
      }
    }

    return inserted;
  } else {
    assert(!dht.root);
    dht.root = alloc<RoutingTable>(dht);
    if (dht.root) {
      goto Lstart;
    }
    printf("- failed alloc\n");
  }

  return nullptr;
} // dht::insert()

std::uint32_t
max_routing_nodes(DHT &) noexcept {
  return std::uint32_t(Bucket::K) * std::uint32_t(sizeof(Key) * 8);
}

} // namespace dht
