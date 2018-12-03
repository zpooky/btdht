#include "Log.h"
#include "dht.h"
#include "timeout.h"
#include <algorithm>
#include <buffer/CircularBuffer.h>
#include <cstdlib>
#include <cstring>
#include <hash/crc.h>
#include <new>
#include <prng/util.h>
#include <util/assert.h>

namespace dht {

static bool
is_cycle(DHT &dht) noexcept {
  Peer *const head = dht.timeout_peer;
  if (head) {
    Peer *it = head;
  Lit:
    if (head) {
      Peer *next = it->timeout_next;
      assertx(it == next->timeout_priv);

      it = next;
      if (it != head) {
        goto Lit;
      }
    } else {
      assertx(head);
    }
  }
  return true;
}

template <std::size_t SIZE>
static bool
randomize(DHT &dht, sp::byte (&buffer)[SIZE]) noexcept {
  sp::byte *it = buffer;
  std::size_t remaining = SIZE;

  while (remaining > 0) {
    auto r = random(dht.random);
    std::size_t length = std::min(sizeof(r), remaining);

    std::memcpy(it, &r, length);
    remaining -= length;
    it += length;
  }
  return true;
}

// See http://www.rasterbar.com/products/libtorrent/dht_sec.html
std::uint32_t
node_id_prefix(const Ip &addr, std::uint32_t seed) noexcept {
  sp::byte octets[8] = {0};
  std::uint32_t size = 0;
  if (addr.type == IpType::IPV6) {
    // our external IPv6 address (network byte order)
    std::memcpy(octets, &addr.ipv6, sizeof(octets));
    // If IPV6
    constexpr sp::byte mask[] = {0x01, 0x03, 0x07, 0x0f,
                                 0x1f, 0x3f, 0x7f, 0xff};
    for (std::size_t i(0); i < sizeof(octets); ++i) {
      octets[i] &= mask[i];
    }
    size = sizeof(octets);
  } else {
    // our external IPv4 address (network byte order)
    // TODO hton
    std::memcpy(octets, &addr.ipv4, sizeof(addr.ipv4));
    // 00000011 00001111 00111111 11111111
    constexpr sp::byte mask[] = {0x03, 0x0f, 0x3f, 0xff};
    for (std::size_t i = 0; i < sizeof(addr.ipv4); ++i) {
      octets[i] &= mask[i];
    }
    size = 4;
  }
  octets[0] |= sp::byte((seed << 5) & sp::byte(0xff));

  return crc32c::encode(octets, size);
}

// // See http://www.rasterbar.com/products/libtorrent/dht_sec.html
bool
is_strict(const Ip &addr, const NodeId &id) noexcept {
  // TODO ipv4
  /*
   * TODO
   * if (is_ip_local(addr)) {
   *   return true;
   * }
   */
  std::uint32_t seed = id.id[19];
  std::uint32_t hash = node_id_prefix(addr, seed);
  // compare the first 21 bits only, so keep bits 17 to 21 only.
  sp::byte from_hash = sp::byte((hash >> 8) & 0xff);
  sp::byte from_node = id.id[2];
  return id.id[0] == sp::byte((hash >> 24) & 0xff) &&
         id.id[1] == sp::byte((hash >> 16) & 0xff) &&
         (from_hash & 0xf8) == (from_node & 0xf8);
}

// See http://www.rasterbar.com/products/libtorrent/dht_sec.html
static bool
randomize(DHT &dht, const Ip &addr, NodeId &id) noexcept {
  // Lstart:
  std::uint32_t seed = random(dht.random) & 0xff;
  std::uint32_t hash = node_id_prefix(addr, seed);
  id.id[0] = sp::byte((hash >> 24) & 0xff);
  id.id[1] = sp::byte((hash >> 16) & 0xff);
  id.id[2] = sp::byte((hash >> 8) & 0xff);
  // if(id.id[0] > 9){
  //   goto Lstart;
  // }
  // need to change all bits except the first 5, xor randomizes the rest of
  // the bits node_id[2] ^= static_cast<byte>(rand() & 0x7);
  for (std::size_t i = 3; i < 19; ++i) {
    id.id[i] = sp::byte(random(dht.random) & 0xff);
  }
  id.id[19] = sp::byte(seed);

  // assertx(id.id[0] <= sp::byte(9));

  return true;
}

void
randomize(prng::xorshift32 &r, NodeId &id) noexcept {
  fill(r, id.id);
  for (std::size_t i = 0; i < 3; ++i) {
    auto pre = uniform_dist(r, std::uint32_t(0), std::uint32_t(9));
    assertx(pre <= 9);
    id.id[i] = sp::byte(pre);
  }
}

// void
// randomize(DHT &dht, NodeId &id) noexcept {
//   randomize(dht, dht.ip.ip, id);
// }

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
    assertx(dht.bad_nodes > 0);
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
      assertx(contact);

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
        assertx(!contact);

        contact = c;
        assertx(contact);

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
  assertx(parent->in_tree == nullptr);

  auto in_tree = alloc<RoutingTable>(dht);
  if (!in_tree) {
    return false;
  }

  auto should_transfer = [&dht, idx](const Node &n) { //
    const bool current_high = bit(n.id, idx);
    const bool in_tree_high = bit(dht.id, idx);
    return current_high == in_tree_high;
  };

  Bucket &bucket = parent->bucket;
  bucket.find_node = Timestamp(0); // reset bucket

  std::size_t moved = 0;
  for (std::size_t i = 0; i < Bucket::K; ++i) {

    Node &contact = bucket.contacts[i];
    if (contact) {
      if (should_transfer(contact)) {
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

        assertx(is_cycle(dht));
        timeout::unlink(dht, &contact);
        assertx(!contact.timeout_next);
        assertx(!contact.timeout_priv);

        const bool eager = false;
        bool replaced /*OUT*/ = false;
        auto *nc = do_insert(dht, in_tree->bucket, contact, eager, replaced);
        assertx(!replaced);
        assertx(nc);

        if (nc) {
          relink(nc);
          // reset
        }
        contact = Node();
        assertx(!contact);

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
//   assertx(lower->type == NodeType::LEAF);
//   RoutingTable *higher = parent->node.higher;
//   assertx(higher->type == NodeType::LEAF);
//
//   parent->~RoutingTable();
//   parent = new (parent) RoutingTable;
//
//   assertx(copy(lower->bucket, parent->bucket));
//   assertx(copy(higher->bucket, parent->bucket));
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
//     assertx(c.timeout_next == nullptr);
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
//   assertx(parent->type == NodeType::NODE);
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
//       assertx(contact.timeout_next == nullptr);
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
  Timestamp created;

  TokenPair();
  operator bool() const noexcept;
};

/*TokenPair*/
TokenPair::TokenPair()
    : ip()
    , token()
    , created(0) {
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
multiple_closest_nodes(DHT &dht, const Key &search, Node **result,
                       std::size_t res_length) noexcept {
  for (std::size_t i = 0; i < res_length; ++i) {
    assertx(result[i] == nullptr);
  }

  Bucket *raw[Bucket::K] = {nullptr};
  sp::CircularBuffer<Bucket *> best(raw);

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
  auto merge = [&dht, &resIdx, &result, res_length](Bucket *b) -> bool { //
    for (std::size_t i = 0; i < res_length; ++i) {
      Node &contact = b->contacts[i];
      if (contact) {
        if (is_good(dht, contact)) {
          result[resIdx++] = &contact;
          if (resIdx == res_length) {
            return true;
          }
        }
      }
    }
    return false;
  };

  {
    Bucket *best_ordered[Bucket::K]{nullptr};
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      // printf("-%p\n", best_ordered[i]);
      assertx(best_ordered[i] == nullptr);
    }
    std::size_t best_idx = 0;

    while (!is_empty(best)) {
      sp::pop_back(best, best_ordered[best_idx++]);
    }

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      if (best_ordered[i]) {

        if (merge(best_ordered[i])) {
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

    /* Using dht.last_activty to better handle a general outgate of network
     * connectivity
     */
    auto resp_timeout = contact.response_activity + config.refresh_interval;
    if (resp_timeout > dht.last_activity) {

      auto req_activity = contact.request_activity + config.refresh_interval;
      if (req_activity > dht.last_activity) {
        return false;
      }
    }
  }
  return true;
}

bool
init(dht::DHT &dht) noexcept {
  // if (!sp::init(dht.recycle_contact_list, 64)) {
  //   return false;
  // }
  // if (!sp::init(dht.recycle_value_list, 64)) {
  //   return false;
  // }
  // if (!sp::init(dht.bootstrap_contacts, 8)) {
  //   return false;
  // }
  if (!randomize(dht, dht.ip.ip, dht.id)) {
    return false;
  }
  if (!tx::init(dht.client)) {
    return false;
  }
  return true;
}

/*public*/
void
multiple_closest(DHT &dht, const NodeId &id, Node **result,
                 std::size_t length) noexcept {
  return multiple_closest_nodes(dht, id.id, result, length);
} // dht::multiple_closest()

void
multiple_closest(DHT &dht, const Infohash &id, Node **result,
                 std::size_t length) noexcept {
  return multiple_closest_nodes(dht, id.id, result, length);
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

  if (dht.id == contact.id) {
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

    /* when we are intree meaning we can add another bucket we do not
     * necessarily need to evict a node that might be late responding to pings
     */
    bool eager_merge = /*!inTree;*/ false;
    bool replaced = false;
    Node *const inserted =
        do_insert(dht, bucket, contact, eager_merge, /*OUT*/ replaced);

    if (inserted) {
      // printf("- insert\n");
      timeout::insert_new(dht, inserted);
      assertx(is_cycle(dht));
      assertx(inserted->timeout_next);
      assertx(inserted->timeout_priv);

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
        assertx(false);
      }
      log::routing::can_not_insert(dht, contact);
    }

    return inserted;
  } else {
    /* Empty tree */
    assertx(!dht.root);
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
