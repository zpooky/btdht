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
bit_compare(const NodeId &id, const Key &cmp, std::size_t length) noexcept {
  for (std::size_t i = 0; i < length; ++i) {
    if (bit(id.id, i) != bit(cmp, i)) {
      return false;
    }
  }
  return true;
}

static bool
debug_correct_level(const DHT &self) noexcept {
  auto it = self.root;
  std::size_t table_cnt = 0;
  std::size_t node_cnt = 0;
  if (it) {
    std::size_t depth = it->depth;
    assertxs(depth == self.root_prefix, depth, self.root_prefix);
    while (it) {
      auto it_next = it;

      while (it_next) {
        assertxs(it_next->depth == depth, it_next->depth, depth,
                 self.root_prefix, length(self.rt_reuse),
                 capacity(self.rt_reuse));

        for (std::size_t i = 0; i < Bucket::K; ++i) {
          auto &contact = it_next->bucket.contacts[i];
          if (is_valid(contact)) {
            ++node_cnt;
            assertxs(bit_compare(self.id, contact.id.id, depth), depth,
                     self.root_prefix, length(self.rt_reuse),
                     capacity(self.rt_reuse));
          }
        }

        // TODO unique set of nodeid

        ++table_cnt;
        it_next = it_next->next;
      }

      it = it->in_tree;
      ++depth;
    }
  }

  std::size_t extra_cnt = 0;
  {
    auto e_it = self.root_extra.root;
    while (e_it) {
      ++extra_cnt;
      e_it = e_it->next;
    }
  }

  // assertxs(table_cnt == length(self.rt_reuse) + extra_cnt, table_cnt,
  // extra_cnt,
  //          length(self.rt_reuse), extra_cnt + length(self.rt_reuse),
  //          self.root_prefix, capacity(self.rt_reuse));

  assertxs(node_cnt == nodes_total(self), node_cnt, nodes_total(self));
  assertxs(nodes_good(self) <= nodes_total(self), //
           nodes_good(self), nodes_total(self));

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
    assertxs(pre <= 9, pre);
    id.id[i] = sp::byte(pre);
  }
}

// void
// randomize(DHT &dht, NodeId &id) noexcept {
//   randomize(dht, dht.ip.ip, id);
// }

static RoutingTable *
find_closest(DHT &self, const NodeId &search, //
             /*OUT*/ bool &in_tree, /*OUT*/ std::size_t &bidx) noexcept {
  in_tree = true;
  RoutingTable *root = self.root;
  bidx = 0;

  while (root) {
    bool high = bit(search, bidx);
    bool self_high = bit(self.id, bidx);
    // in_tree is true if search so far share the same prefix with self.id
    in_tree &= (self_high == high);

    if (in_tree) {
      if (root->in_tree) {
        /* More precise match */
        root = root->in_tree;
      } else {
        return root;
      }
    } else {
      return root;
    }

    ++bidx;
  }

  return root;
}

template <typename T>
static void
dealloc_RoutingTable(DHT &self, T *recycle) {
  assertx(recycle);
  assertx(!recycle->next);
  assertx(!recycle->in_tree);

  // TODO this is not correctly impl in heap
  auto fres = find(self.rt_reuse, recycle);
  assertx(fres);
  assertx(*fres == recycle);
  if (fres) {
    const auto bdepth = recycle->depth;

    sp::dstack_node<RoutingTable *> *tmp = nullptr;
    if (pop(self.root_extra, tmp)) {
      assertxs(tmp->value, self.root_prefix, length(self.rt_reuse),
               capacity(self.rt_reuse));

      delete recycle;

      *fres = recycle = tmp->value;
      reclaim(self.root_extra, tmp);
    } else {
      recycle->~T();
      new (recycle) RoutingTable(-1);
    }

    auto res = update_key(self.rt_reuse, fres);
    assertxs(*res == recycle, bdepth, recycle->depth);
  } else {
    delete recycle;
  }
}

static void
reset(DHT &self, Node &contact) noexcept {
  if (!contact.good) {
    assertx(self.bad_nodes > 0);
    self.bad_nodes--;
    contact.good = true;
  }

  assertxs(self.total_nodes > 0, self.total_nodes);
  if (self.total_nodes > 0) {
    --self.total_nodes;
  }

  contact = Node();
}

static void
unlink_reset(DHT &self, Node &contact) {
  if (is_valid(contact)) {
    timeout::unlink(self, &contact);
    assertx(!contact.timeout_next);
    assertx(!contact.timeout_priv);
    reset(self, contact);
  }
}

static bool
dequeue_root_dtor(DHT &self, RoutingTable *const subject) noexcept {
  assertx(subject);
  if (self.root == subject) {
    self.root = subject->next;
  } else {
    auto it = self.root;
    while (it) {
      if (it->next == subject) {
        it->next = subject->next;
        break;
      }
      it = it->next;
    }

    if (it == nullptr) {
      return false;
    }
  }

  auto &bucket = subject->bucket;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    auto &contact = bucket.contacts[i];
    unlink_reset(self, contact);
  }

  if (!self.root) {
    assertxs(subject->in_tree, subject->depth, self.root_prefix,
             length(self.rt_reuse), capacity(self.rt_reuse));
    self.root = subject->in_tree;
  }

  subject->in_tree = nullptr;
  subject->next = nullptr;

  // TODO check this when inserting
  self.root_prefix++;

  return true;
}

static void
enqueue(RoutingTable *leaf, RoutingTable *next) noexcept {
  assertx(leaf);
  assertx(next);
  RoutingTable *it = leaf;
  while (it) {
    if (!it->next) {
      it->next = next;
      break;
    }
    it = it->next;
  }
}

enum class AllocType { REC, PLAIN };

static RoutingTable *
alloc_RoutingTable(DHT &self, std::size_t depth, AllocType aType) noexcept {
  RoutingTable **head = peek_head(self.rt_reuse);
  if (head) {
    auto h = *head;
    assertx(h);

    if (h->depth < 0) {
      h->~RoutingTable();
      new (h) RoutingTable(depth);
      auto res = update_key(self.rt_reuse, head);
      assertxs(res, depth);
      assertxs(*res == h, depth);
      return h;
    }

    if (!is_full(self.rt_reuse)) {
      goto Lbah;
    }

    if (h->depth < depth) { // TODO fix cmp
      // TODO only do this when we are in high prio: (modify alloc prototype)
      if (aType == AllocType::REC && (h->depth + 1) == depth) {
        auto result = new RoutingTable(depth);
        if (result) {
          if (!push(self.root_extra, result)) {
            delete result;
            result = nullptr;
          }
        }
        return result;
      }

      if (!dequeue_root_dtor(self, h)) {
        assertxs(false, h->depth, h->in_tree, self.root_prefix,
                 length(self.rt_reuse), capacity(self.rt_reuse)); // TODO fails
        return nullptr;
      }
      // XXX migrate of possible good contacts in $h to empty/bad linked
      //     RoutingTable contacts. timeout contact

      const auto h_depth = h->depth;

      h->~RoutingTable();
      new (h) RoutingTable(depth);

      auto res = update_key(self.rt_reuse, head);
      assertxs(res, h_depth, depth);
      assertxs(*res == h, h_depth, depth);
      // TODO assertxs(res != head, h_depth, depth);??

      return h;
    }
  }

Lbah:
  if (!is_full(self.rt_reuse)) {
    auto result = new RoutingTable(depth);
    if (result) {
      auto r = insert(self.rt_reuse, result);
      assertx(r);
      assertx(*r == result);
    }

    return result;
  }

  return nullptr;
}

static Node *
bucket_insert(DHT &dht, Bucket &bucket, const Node &c, //
              bool eager, /*OUT*/ bool &replaced) noexcept {
  replaced = false;

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (!is_valid(contact)) {
      contact = c;

      return &contact;
    }
  }

  if (eager) {
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = bucket.contacts[i];
      assertx(is_valid(contact));
      if (!is_good(dht, contact)) {
        timeout::unlink(dht, &contact);
        reset(dht, contact);
        assertx(!is_valid(contact));

        contact = c;
        assertx(is_valid(contact));

        return &contact;
      }
    }
  }

  return nullptr;
}

static Node *
table_insert(DHT &self, RoutingTable &table, const Node &c, //
             bool eager, /*OUT*/ bool &replaced) noexcept {
  RoutingTable *it = &table;
  while (it) {
    Node *result = bucket_insert(self, it->bucket, c, eager, replaced);
    if (result) {
      return result;
    }

    it = it->next;
  }

  return nullptr;
}

static Node *
find(RoutingTable &table, const Key &id) noexcept {
  RoutingTable *it = &table;

  while (it) {
    Bucket &bucket = it->bucket;

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = bucket.contacts[i];
      if (is_valid(contact)) {

        if (contact.id == id) {
          return &contact;
        }
      }
    }

    it = it->next;
  }

  return nullptr;
}

static Node *
find(RoutingTable &table, const NodeId &id) noexcept {
  return find(table, id.id);
}

static bool
node_reseat(DHT &self, Node &source, Bucket &dest) noexcept {
  assertx(is_valid(source));

  Node dummy;

  const bool eager = false;
  bool replaced /*OUT*/ = false;
  auto *nc = bucket_insert(self, dest, dummy, eager, /*OUT*/ replaced);
  assertx(!replaced);
  if (nc) {

    Node *const priv = source.timeout_priv;
    assertx(priv);
    Node *const next = source.timeout_next;
    assertx(next);

    auto relink = [priv, next](Node *c) {
      c->timeout_priv = priv;
      c->timeout_next = next;
      if (priv)
        priv->timeout_next = c;
      if (next)
        next->timeout_priv = c;
    };

    timeout::unlink(self, &source);
    assertx(!source.timeout_next);
    assertx(!source.timeout_priv);

    *nc = source;
    relink(nc);
    assertx(is_valid(*nc));

    // reset
    source = Node();
    assertx(!is_valid(source));
  } // if

  return nc;
}

static RoutingTable *
split_transfer(DHT &self, RoutingTable *better, //
               Bucket &subject, std::size_t level) {
  RoutingTable *better_it = better;

  auto should_transfer = [&self, level](const Node &n) {
    const bool current_high = bit(n.id, level);
    const bool in_tree_high = bit(self.id, level);
    return current_high == in_tree_high;
  };

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = subject.contacts[i];
    if (is_valid(contact)) {
      if (should_transfer(contact)) {

        if (!better_it) {
          better_it = alloc_RoutingTable(self, level, AllocType::REC);
          if (!better_it) {
            goto Lreset;
          }
        }

        assertx(!better_it->next);
        while (!node_reseat(self, /*src*/ contact, better_it->bucket)) {
          assertx(!better_it->next);
          // TODO what happens if it tries to reallocate a table node we are
          // currently using?
          better_it->next = alloc_RoutingTable(self, level, AllocType::REC);
          if (!better_it->next) {
            goto Lreset;
          }
          better_it = better_it->next;
        } // while

      } // should_transfer
    }   // if is_valid
  }     // for

  return better_it;
Lreset:
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    if (is_valid(subject.contacts[i])) {
      reset(self, subject.contacts[i]);
    }
  }

  // level=0, self.root_prefix=0, length(self.rt_reuse)=32,
  // capacity(self.rt_reuse)=32
  assertxs(false, level, self.root_prefix, length(self.rt_reuse),
           capacity(self.rt_reuse));
  return better_it;
}

static bool
split(DHT &self, RoutingTable *target_root, std::size_t level) noexcept {
  assertx(target_root);
  /* If there already is a target_root->in_tree node then we already have
   * performed the split and should not get here again.
   */
  // assertxs(target_root->depth == (level + 1), target_root->depth, level);
  const std::size_t target_depth = target_root->depth;

  RoutingTable *better_root = nullptr;
  RoutingTable *better_it = nullptr;

  RoutingTable *target_priv = nullptr;
  RoutingTable *target_it = target_root;

  while (target_it) {
    assertx(target_depth == target_it->depth); // TODO fails
    assertx(target_it->in_tree == nullptr);
    Bucket &bucket = target_it->bucket;

    better_it =
        split_transfer(self, /*dest*/ better_it, /*src*/ bucket, level + 1);
    if (!better_root) {
      better_root = better_it;
    }

    std::size_t present_cnt = 0;
    /*compact with $target_priv if present*/
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = bucket.contacts[i];
      if (is_valid(contact)) {
        if (target_priv) {
          node_reseat(self, contact, target_priv->bucket);
        }
      }

      if (is_valid(contact)) {
        ++present_cnt;
      }
    }

    RoutingTable *const t_next = target_it->next;
    if (present_cnt > 0 || target_it == target_root) {
      target_priv = target_it;
    } else {
      target_it->in_tree = nullptr;
      target_it->next = nullptr;
      dealloc_RoutingTable(self, target_it);
      if (target_priv) {
        target_priv->next = t_next;
      }
    }

    target_it = t_next;
  }
  assertx(better_root);
  assertx(!target_root->in_tree);

  target_root->in_tree = better_root;

  // TODO log::routing::split(dht, *higher, *lower);
  return true;
}

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
multiple_closest_nodes(DHT &self, const Key &search, Node **result,
                       std::size_t res_length) noexcept {
  for (std::size_t i = 0; i < res_length; ++i) {
    assertx(result[i] == nullptr);
  }

  RoutingTable *raw[Bucket::K] = {nullptr};
  sp::CircularBuffer<RoutingTable *> best(raw);

  /* Fill buffer */
  RoutingTable *root = self.root;
  std::size_t idx = 0;
  const std::size_t max = shared_prefix(self.id, search);
Lstart:
  if (root) {
    sp::push_back(best, root);
    if (idx++ < max) {
      root = root->in_tree;
      goto Lstart;
    }
  }

  std::size_t resIdx = 0;
  auto merge = [&](RoutingTable &table) -> bool {
    RoutingTable *it = &table;

    while (it) {
      auto &b = it->bucket;

      for (std::size_t i = 0; i < res_length; ++i) {
        Node &contact = b.contacts[i];
        if (is_valid(contact)) {

          if (is_good(self, contact)) {
            result[resIdx++] = &contact;

            if (resIdx == res_length) {
              /* Full */
              return true;
            }
          }
        }
      } // for

      it = it->next;
    }

    return false;
  };

  {
    RoutingTable *best_ordered[Bucket::K]{nullptr};
    std::size_t best_idx = 0;

    /* Reverse order */
    while (!is_empty(best)) {
      sp::pop_back(best, best_ordered[best_idx++]);
    }

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      if (best_ordered[i]) {

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
should_mark_bad(DHT &self, Node &contact) noexcept {
  // XXX
  return !is_good(self, contact);
}

bool
is_good(const DHT &dht, const Node &contact) noexcept {
  const Config &config = dht.config;
  // XXX configurable non arbitrary limit?
  if (contact.ping_outstanding > 2) {

    /* Using dht.last_activty to better handle a general outage of network
     * connectivity
     */
    auto resp_timeout = contact.remote_activity + config.refresh_interval;
    if (resp_timeout > dht.last_activity) {
      return false;
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

    return find(*leaf, search);
  }

  return nullptr;
} // dht::find_contact()

const Bucket *
bucket_for(DHT &dht, const NodeId &id) noexcept {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, id, inTree, idx);
  return leaf ? &leaf->bucket : nullptr;
}

static bool
can_split(const Bucket &bucket, std::size_t idx) {
  std::size_t bits[2] = {0};
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    const Node &c = bucket.contacts[i];
    if (is_valid(c)) {
      std::size_t bit_idx = bit(c.id, idx) ? 0 : 1;
      bits[bit_idx]++;
    }
  }

  if (bits[0] > 0 && bits[1] > 0) {
    return true;
  }

  return false;
}

static bool
can_split(const RoutingTable &table, std::size_t idx) {
  const RoutingTable *it = &table;
  while (it) {
    if (can_split(it->bucket, idx)) {
      return true;
    }
    it = it->next;
  }

  return false;
}

Node *
insert(DHT &self, const Node &contact) noexcept {
  if (!is_valid(contact.id)) {
    return nullptr;
  }

  if (self.id == contact.id) {
    return nullptr;
  }

Lstart:
  /* inTree means that contact share the same prefix with self, longer than
   * $bidx.
   */
  bool inTree = false;
  std::size_t bidx = 0;

  RoutingTable *const leaf =
      find_closest(self, contact.id, /*OUT*/ inTree, /*OUT*/ bidx);
  if (leaf) {
    auto sp = shared_prefix(self.id, contact.id.id);
    if (sp < self.root_prefix) {
      assertxs(!find(*leaf, contact.id), sp);
      return nullptr;
    }

    {
      /*check if already present*/
      Node *const existing = find(*leaf, contact.id);
      if (existing) {
        return existing;
      }
    }

    /* when we are intree meaning we can add another bucket we do not
     * necessarily need to evict a node that might be late responding to pings
     */
    bool eager_merge = /*!inTree;*/ false;
    bool replaced = false;
    assertx(debug_correct_level(self));
    Node *const inserted =
        table_insert(self, *leaf, contact, eager_merge, /*OUT*/ replaced);

    if (inserted) {
      timeout::insert_new(self, inserted);

      log::routing::insert(self, *inserted);
      if (!replaced) {
        ++self.total_nodes;
      }
      assertx(debug_correct_level(self));
    } else {
      assertx(debug_correct_level(self));
      if (can_split(*leaf, bidx)) {
        if (split(self, /*parent*/ leaf, bidx)) {
          assertx(debug_correct_level(self));
          // XXX make better
          goto Lstart;
        }
      } else {
        auto next = alloc_RoutingTable(self, bidx, AllocType::PLAIN);
        if (next) {
          enqueue(leaf, next);
          assertx(debug_correct_level(self));
          goto Lstart;
        }
      }
      log::routing::can_not_insert(self, contact);
    }

    return inserted;
  } else {
    /* Empty tree */
    assertx(!self.root);
    assertx(is_empty(self.rt_reuse));

    self.root = alloc_RoutingTable(self, 0, AllocType::PLAIN);
    if (self.root) {
      goto Lstart;
    }
  }

  return nullptr;
} // dht::insert()

std::uint32_t
max_routing_nodes(DHT &) noexcept {
  return std::uint32_t(Bucket::K) * std::uint32_t(sizeof(Key) * 8);
}

std::uint32_t
nodes_good(const DHT &self) noexcept {
  return nodes_total(self) - nodes_bad(self);
}

std::uint32_t
nodes_total(const DHT &self) noexcept {
  return self.total_nodes;
}

std::uint32_t
nodes_bad(const DHT &self) noexcept {
  return self.bad_nodes;
}

void
bootstrap_insert(DHT &self, const Contact &remote) noexcept {
  if (!test(self.bootstrap_filter, remote)) {
    if (push(self.bootstrap, remote)) {
      insert(self.bootstrap_filter, remote);
    }
  }
}

void
bootstrap_insert(DHT &self, sp::dstack_node<Contact> *in) noexcept {
  assertx(in);
  if (!test(self.bootstrap_filter, in->value)) {
    push(self.bootstrap, in);

    insert(self.bootstrap_filter, in->value);
  }
}

void
bootstrap_insert_force(DHT &self, const Contact &remote) noexcept {
  if (push(self.bootstrap, remote)) {
    insert(self.bootstrap_filter, remote);
  }
}

void
bootstrap_insert_force(DHT &self, sp::dstack_node<Contact> *in) noexcept {
  assertx(in);
  if (in) {
    push(self.bootstrap, in);
    insert(self.bootstrap_filter, in->value);
  }
}

void
bootstrap_reset(DHT &self) noexcept {
  auto &filter = self.bootstrap_filter;
  auto &set = filter.bitset;
  std::memset(set.raw, 0, sizeof(set.raw));
}

} // namespace dht
