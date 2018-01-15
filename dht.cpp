#include "Log.h"
#include "dht.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>
#include <util/CircularBuffer.h>

namespace dht {

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

      timeout::append_all(dht, &contact);
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

        timeout::append_all(dht, &contact);

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

        timeout::unlink(dht, &contact);

        bool eager = false;
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

static bool
bit_compare(const NodeId &id, const Key &cmp, std::size_t length) noexcept {
  for (std::size_t i = 0; i < length; ++i) {
    if (bit(id.id, i) != bit(cmp, i)) {
      return false;
    }
  }
  return true;
}

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

  auto is_in_tree = [&dht, &search](std::size_t idx) {
    return bit_compare(dht.id, search, idx) == 0;
  };

  RoutingTable *root = dht.root;
  std::size_t idx = 0;
Lstart:
  if (root) {
    sp::push_back(best, &root->bucket);
    // if (find(root->bucket, search)) {
    //   printf("found at idx[%zu]\n", idx);
    // }
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
  if (!sp::init(dht.contact_list, 64)) {
    return false;
  }
  if (!sp::init(dht.value_list, 64)) {
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
  }

  return nullptr;
} // dht::insert()

} // namespace dht

namespace timeout {
template <typename T>
static T *
last(T *node) noexcept {
Lstart:
  if (node) {
    if (node->timeout_next) {
      node = node->timeout_next;
      goto Lstart;
    }
  }
  return node;
} // timeout::last()

template <typename T>
void
internal_unlink(T *&head, T *const contact) noexcept {
  T *const priv = contact->timeout_priv;
  T *const next = contact->timeout_next;

  assert(priv);
  assert(next);

  if (priv)
    priv->timeout_next = next;

  if (next)
    next->timeout_priv = priv;

  if (head == contact)
    head = next;

  contact->timeout_next = nullptr;
  contact->timeout_priv = nullptr;
}

void
unlink(dht::Node *&head, dht::Node *contact) noexcept {
  return internal_unlink(head, contact);
} // timeout::unlink()

void
unlink(dht::DHT &ctx, dht::Node *contact) noexcept {
  return unlink(ctx.timeout_node, contact);
} // timeout::unlink()

void
unlink(dht::Peer *&head, dht::Peer *peer) noexcept {
  return internal_unlink(head, peer);
} // timeout::unlink()

template <typename T>
void
internal_append_all(T *&head, T *const node) noexcept {
  if (!head) {
    T *const lst = last(node);

    lst->timeout_next = node;
    node->timeout_priv = lst;
    node->timeout_next = node;

    head = node;
  } else {
    T *const l = last(node);

    T *const priv = head->timeout_priv;
    assert(priv);
    node->timeout_priv = priv;
    priv->timeout_next = node;

    T *const next = head;
    l->timeout_next = next;
    next->timeout_priv = l;
  }
}

void
append_all(dht::DHT &ctx, dht::Node *node) noexcept {
  return internal_append_all(ctx.timeout_node, node); //<--
} // timeout::append_all()

static void
append_all(dht::DHT &ctx, dht::Peer *peer) noexcept {
  return internal_append_all(ctx.timeout_peer, peer);
} // timeout::append_all()

} // namespace timeout

namespace lookup {
dht::KeyValue *
lookup(dht::DHT &dht, const dht::Infohash &infohash) noexcept {
  auto find_haystack = [](dht::KeyValue *current, const dht::Infohash &id) {
    // XXX tree?
  Lstart:
    if (current) {
      if (id == current->id) {
        return current;
      }
      current = current->next;
      goto Lstart;
    }

    return current;
  };

  auto is_expired = [&dht](dht::Peer &peer) {
    time_t peer_activity = peer.activity;

    // Determine to age if end of life is higher than now and make sure that
    // we have an internet connection by checking that we have received any
    // updates at all
    dht::Config config;
    time_t peer_eol = peer_activity + config.peer_age_refresh;
    if (peer_eol < dht.now) {
      if (dht.last_activity > peer_eol) {
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

  dht::KeyValue *const needle = find_haystack(dht.lookup_table, infohash);
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
      } else {
        previous = it;
      }

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
} // lookup::lookup()

bool
insert(dht::DHT &dht, const dht::Infohash &infohash,
       const Contact &contact) noexcept {

  auto new_table = [&dht, infohash]() -> dht::KeyValue * {
    auto result = new dht::KeyValue(infohash, dht.lookup_table);
    if (result) {
      dht.lookup_table = result;
    }
    return result;
  };

  auto add_peer = [&dht](dht::KeyValue &s, const Contact &c) {
    auto p = new dht::Peer(c, dht.now, s.peers);
    if (p) {
      s.peers = p;
      return true;
    }
    return false;
  };

  auto find = [](dht::KeyValue &t, const Contact &s) {
    dht::Peer *it = t.peers;
  Lstart:
    if (it) {
      if (it->contact == s) {
        return it;
      }
      it = it->next;
      goto Lstart;
    }

    return (dht::Peer *)nullptr;
  };

  dht::KeyValue *table = lookup(dht, infohash);
  if (!table) {
    table = new_table();
  }

  if (table) {
    dht::Peer *const existing = find(*table, contact);
    if (existing) {
      // TODO timeout::unlink_x(dht, existing);
      existing->activity = dht.now;
      timeout::append_all(dht, existing);

      return true;
    } else if (add_peer(*table, contact)) {
      log::peer_db::insert(dht, infohash, contact);

      return true;
    }
    if (!table->peers) {
      // TODO if add false and create needle reclaim needle
    }
  }

  return false;
} // lookup::insert()

void
mint_token(dht::DHT &, Ipv4, dht::Token &t) noexcept {
  dht::randomize(t.id);
  // TODO
} // lookup::mint_token()

bool
valid(dht::DHT &, const dht::Token &) noexcept {
  // TODO
  return true;
} // lookup::valid()

} // namespace lookup
