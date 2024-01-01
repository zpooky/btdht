#include "routing_table.h"
#include "Log.h"
#include "timeout.h"
#include <algorithm>
#include <buffer/CircularBuffer.h>
#include <cstdlib>
#include <cstring>
#include <hash/crc.h>
#include <heap/binary.h>
#include <new>
#include <prng/util.h>
#include <util/assert.h>

namespace dht {

//=====================================
/*dht::Bucket*/
Bucket::Bucket() noexcept
    : contacts()
    , length(0) {
}

Bucket::~Bucket() noexcept {
}

//=====================================
/*dht::RoutingTable*/
RoutingTable::RoutingTable(ssize_t d) noexcept
    : depth(d)
    , in_tree()
    , bucket()
    , next(nullptr) {
}

RoutingTable::~RoutingTable() noexcept {
#if 0
  if (in_tree) {
    delete in_tree;
    in_tree = nullptr;
  }
#endif
}

static std::size_t
debug_bucket_count(const Bucket &b) {
  std::size_t result = 0;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    const auto &current = b.contacts[i];
    if (is_valid(current)) {
      ++result;
    }
  }

  return result;
}

bool
is_empty(const RoutingTable &root) noexcept {
  assertx(debug_bucket_count(root.bucket) == root.bucket.length);
  return root.bucket.length == 0;
}

#if 0
bool
operator<(const RoutingTable &f, std::size_t depth) noexcept {
  return f.depth < depth;
}

bool
operator<(const RoutingTable &f, const RoutingTable &s) noexcept {
  return f.depth < s.depth;
}
#endif

bool
RoutingTableLess::operator()(const RoutingTable *f,
                             std::size_t depth) const noexcept {
  assertx(f);
  if (f->depth < 0) {
    return true;
  }

  assertx(f->depth >= 0);
  return (std::size_t)f->depth < depth;
}

bool
RoutingTableLess::operator()(const RoutingTable *f,
                             const RoutingTable *s) const noexcept {
  assertx(f);
  assertx(s);
  return f->depth < s->depth;
}

// ========================================
DHTMetaRoutingTable::DHTMetaRoutingTable(prng::xorshift32 &r,
                                         timeout::TimeoutBox &_tb, Timestamp &n,
                                         const dht::NodeId &i,
                                         const dht::Config &c)
    : root(nullptr)
    , rt_reuse(160)
    , random{r}
    , tb{_tb}
    , id{i}
    , now{n}
    , config(c)
    , total_nodes(0)
    , bad_nodes(0) {
}

DHTMetaRoutingTable::~DHTMetaRoutingTable() {
  RoutingTable *it = this->root;
  while (it) {
    RoutingTable *in_tree = it->in_tree;

    while (it) {
      RoutingTable *it_next = it->next;

      // fprintf(stderr, "it[%p]\n", (void *)it);
      assertx(debug_bucket_count(it->bucket) == it->bucket.length);
      if (this->tb.timeout) {
        for (std::size_t i = 0; i < Bucket::K; ++i) {
          Node &node = it->bucket.contacts[i];
          if (is_valid(node)) {
            timeout::unlink(*this->tb.timeout, &node);
          }
        } // for
      }

      delete it;
      this->root = it = it_next;
    } // while

    this->root = it = in_tree;
  } // while

  this->root = NULL;
}

// ========================================
void
debug_for_each(const DHTMetaRoutingTable &self, void *closure,
               void (*f)(void *, const DHTMetaRoutingTable &,
                         const Node &)) noexcept {
  const RoutingTable *it = self.root;
  while (it) {

    const RoutingTable *it_next = it;
    while (it_next) {

      assertx(debug_bucket_count(it_next->bucket) == it_next->bucket.length);
      for (std::size_t i = 0; i < Bucket::K; ++i) {
        const Node &node = it_next->bucket.contacts[i];
        if (is_valid(node)) {
          f(closure, self, node);
        }
      } // for

      it_next = it_next->next;
    } // while

    it = it->in_tree;
  } // while
}

static std::size_t
debug_count_good(const DHTMetaRoutingTable &self) {
  std::size_t result = 0;

  auto xx = [](void *res, const DHTMetaRoutingTable &self, const Node &node) {
    if (is_valid(node) && is_good(self, node)) {
      std::size_t *r = (std::size_t *)res;
      (*r)++;
    }
  };

  debug_for_each(self, &result, xx);

  return result;
}

std::size_t
debug_levels(const DHTMetaRoutingTable &self) noexcept {
  std::size_t result = 0;
  const RoutingTable *it = self.root;
  while (it) {
    ++result;
    it = it->in_tree;
  }

  return result;
}

static bool
prefix_compare(const NodeId &id, const Key &cmp, std::size_t length) noexcept {
  const std::size_t bytes = length / 8;
  for (std::size_t i = 0; i < bytes; ++i) {
    if (id.id[i] != cmp[i]) {
      return false;
    }
  }

  for (std::size_t i = bytes * 8; i < length; ++i) {
    if (bit(id.id, i) != bit(cmp, i)) {
      return false;
    }
  }
  return true;
}

static bool
debug_bucket_is_valid(const RoutingTable *it) {
  const auto &b = it->bucket;
  assertxs(debug_bucket_count(it->bucket) == it->bucket.length,
           debug_bucket_count(it->bucket), it->bucket.length);
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    is_valid(b.contacts[i]);
  }
  return true;
}

bool
debug_assert_all(const DHTMetaRoutingTable &self) {
  auto it = self.root;
  while (it) {
    auto it_next = it;
    while (it_next) {
      assertx(debug_bucket_is_valid(it_next));
      it_next = it_next->next;
    }
    it = it->in_tree;
  }
  return true;
}

static bool
debug_find(const DHTMetaRoutingTable &self, RoutingTable *next) {
  assertx(next);

  auto it = self.root;
  while (it) {
    auto it_next = it;
    while (it_next) {
      if (it_next == next) {
        return true;
      }
      it_next = it_next->next;
    }
    it = it->in_tree;
  }

  return false;
}

static void
debug_print(const char *ctx, const DHTMetaRoutingTable &self) noexcept {
  // #if 0
  if (ctx) {
    printf("%s, length(heap): %zu\n", ctx, length(self.rt_reuse));
  }
  auto it = self.root;
  while (it) {
    auto it_next = it;
    while (it_next) {
      printf("%p[%zd, nodes(%zu)]->", (void *)it_next, it_next->depth, it_next->bucket.length);
      it_next = it_next->next;
    }
    printf("\n");
    it = it->in_tree;
  }
  printf("\n");
  // #endif
}

static bool
debug_correct_level(const DHTMetaRoutingTable &self) noexcept {
  auto it = self.root;
  std::size_t table_cnt = 0;
  std::size_t node_cnt = 0;
  if (it) {
    auto depth = it->depth;
    while (it) {
      auto it_next = it;
      std::size_t n = 0;
      std::size_t lvl_nodes = 0;

      while (it_next) {
        assertxs(it_next->depth == depth, it_next->depth, depth,
                 length(self.rt_reuse), capacity(self.rt_reuse), n);
        assertx(debug_bucket_count(it_next->bucket) == it_next->bucket.length);

        for (std::size_t i = 0; i < Bucket::K; ++i) {
          auto &contact = it_next->bucket.contacts[i];
          if (is_valid(contact)) {
            ++lvl_nodes;
            if (!prefix_compare(self.id, contact.id.id, depth)) {

              printf("\n%s\n", to_string(self.id));
              printf("%s\nshared[%zu]\n", //
                     to_string(contact.id), rank(self.id, contact.id));

              assertxs(prefix_compare(self.id, contact.id.id, depth), depth,
                       length(self.rt_reuse), capacity(self.rt_reuse));
            }
          }
        } // for

        // TODO unique set of nodeid

        ++table_cnt;
        it_next = it_next->next;
        if (it_next) {
          assertx(!it_next->in_tree);
        }
        ++n;
      } // while
      // printf(" lvl[%zu]: %zu\n", depth, lvl_nodes);
      node_cnt += lvl_nodes;

      it = it->in_tree;
      ++depth;
    } // while
  }

  assertxs(node_cnt == nodes_total(self), node_cnt, nodes_total(self));
#if 0
  if (self.timeout) {
    assertxs(node_cnt == timeout::debug_count_nodes(*self.timeout), node_cnt,
             timeout::debug_count_nodes(*self.timeout));
  }
#endif
  assertxs(nodes_good(self) <= nodes_total(self), nodes_good(self),
           nodes_total(self));

  return true;
}

static bool
debug_routing_level_iscorrect(DHTMetaRoutingTable &self, RoutingTable *root) {
  assertx(root->depth >= 0);

  for (RoutingTable *it = root; it; it = it->next) {
    assertxs(it->depth == root->depth, it->depth, root->depth);

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &node = it->bucket.contacts[i];
      if (is_valid(node)) {
        if (!prefix_compare(self.id, node.id.id, (std::size_t)root->depth)) {
          return false;
        }
        // TODO compare prefix up to root->depth with self.id
      }
    } // for
  }   // for

  return true;
}

static RoutingTable *
find_closest(DHTMetaRoutingTable &self, const NodeId &search,
             /*OUT*/ bool &in_tree, /*OUT*/ std::size_t &bidx) noexcept {
  in_tree = true;
  RoutingTable *root = self.root;
  bidx = 0;
  if (root) {
    assertx(root->depth >= 0);
    bidx = (std::size_t)root->depth;
  }

  while (root) {
    assertx(debug_routing_level_iscorrect(self, root));
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

static void
dealloc_RoutingTable(DHTMetaRoutingTable &self, RoutingTable *recycle) {
  assertx(recycle);
  assertx(!recycle->next);
  assertx(!recycle->in_tree);

  auto &bucket = recycle->bucket;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    assertx(!is_valid(bucket.contacts[i]));
  }

  // TODO this is not correctly impl in heap
  auto fres = find(self.rt_reuse, recycle);
  if (fres) {
    assertx(*fres == recycle);
    const auto bdepth = recycle->depth;

    recycle->~RoutingTable();
    new (recycle) RoutingTable(-1);

    auto res = update_key(self.rt_reuse, fres);
    assertxs(*res == recycle, bdepth, recycle->depth);
  } else {
    assertx(false);
  }
}

static void
reset(DHTMetaRoutingTable &self, Node &contact) noexcept {
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
  assertx(!is_valid(contact));
}

static bool
timeout_unlink_reset(DHTMetaRoutingTable &self, Node &contact) {
  if (is_valid(contact)) {
    if (self.tb.timeout) {
      timeout::unlink(*self.tb.timeout, &contact);
    }

    if (contact.good) {
      for_each(self.retire_good, [&contact](auto t) { //
        auto retire_good = std::get<0>(t);
        retire_good(std::get<1>(t), contact.contact);
      });
    }

    reset(self, contact);

    return true;
  }

  return false;
}

bool
debug_timeout_unlink_reset(DHTMetaRoutingTable &self, Node &contact) {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *root = find_closest(self, contact.id, inTree, idx);
  assertx(is_valid(contact));

  for (RoutingTable *it = root; it; it = it->next) {

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &tmp = it->bucket.contacts[i];

      if (tmp.id == contact.id) {
        timeout_unlink_reset(self, contact);
        assertx(!is_valid(contact));
        --it->bucket.length;
        return true;
      }
    }
  }

  return false;
}

static bool
dequeue_root(DHTMetaRoutingTable &self, RoutingTable *const needle) noexcept {
  assertx(needle);
  assertx(self.root);

#if 0
  auto in_tree = self.root->in_tree;
  if (self.root == needle) {
    self.root = needle->next;
    if (self.root == nullptr) {
      self.root = in_tree;
    } else {
      self.root->in_tree = in_tree;
    }
  } else {
    auto it = self.root;
    while (it) {
      if (it->next == needle) {
        it->next = needle->next;
        break;
      }
      it = it->next;
    }

    if (it == nullptr) {
      return false;
    }
  }
#else
  bool found = false;
  RoutingTable *it = self.root;
  RoutingTable *parent = nullptr;
  while (it) {
    RoutingTable *const current_level = it;
    RoutingTable *paralell_priv = nullptr;
    while (it) {
      if (it == needle) {
        if (paralell_priv) {
          paralell_priv->next = needle->next;
        } else if (needle->next) {
          if (parent) {
            parent->in_tree = needle->next;
          } else {
            self.root = needle->next;
          }
          needle->next->in_tree = current_level->in_tree;
        } else {
          if (!parent) {
            self.root = needle->in_tree;
          } else {
            // we remove a none root node
            // (there is a parent that should be removed)
            assertx(false);
            return false;
          }
        }
        found = true;
        goto Lout;
      }
      paralell_priv = it;
      it = it->next;
    } // while
    parent = it;
    it = current_level->in_tree;
  } // while
Lout:
  if (!found) {
    assertx(false);
    return false;
  }
#endif

  auto &bucket = needle->bucket;
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    auto &contact = bucket.contacts[i];
    auto tot = nodes_total(self);

    if (timeout_unlink_reset(self, contact)) {
      bucket.length--;
      assertxs(nodes_total(self) == tot - 1, nodes_total(self), tot - 1);
    }
  }

  needle->in_tree = nullptr;
  needle->next = nullptr;

  return true;
}

static void
enqueue_level(RoutingTable *leaf, RoutingTable *next) noexcept {
  assertx(leaf);
  assertx(next);
#if 0
  RoutingTable *it = leaf;
  while (it) {
    if (!it->next) {
      it->next = next;
      break;
    }
    it = it->next;
  }
#endif
  assertx(!next->next);
  next->next = leaf->next;
  leaf->next = next;
}

enum class AllocType { REC, PLAIN };

static RoutingTable *
alloc_RoutingTable(DHTMetaRoutingTable &self, std::size_t depth,
                   AllocType aType) noexcept {
  RoutingTable **head = peek_head(self.rt_reuse);
  if (head) {
    auto h = *head;
    assertx(h);

    if (h->depth < 0) {
      h->~RoutingTable();
      new (h) RoutingTable((ssize_t)depth);
      auto res = update_key(self.rt_reuse, head);
      assertxs(res, depth);
      assertxs(*res == h, depth);
      return h;
    }

    if (!is_full(self.rt_reuse)) {
      goto Lbah;
    }

    // TODO fix cmp
    if (std::size_t(h->depth) < depth) {
      // TODO only do this when we are in high prio: (modify alloc prototype)
      if (aType == AllocType::REC && std::size_t(h->depth + 1) == depth) {
        auto result = new RoutingTable(depth);
        if (result) {
          auto res = insert(self.rt_reuse, result);
          assertx(res);
          assertx(*res == result);
          /*
           * if (!push(self.root_extra, result)) {
           *   delete result;
           *   result = nullptr;
           * }
           */
        }
        return result;
      }

      if (!dequeue_root(self, h)) {
        // TODO fails
        assertxs(false, h->depth, h->in_tree, length(self.rt_reuse),
                 capacity(self.rt_reuse));
        return nullptr;
      }
      // XXX migrate of possible good contacts in $h to empty/bad linked
      //     RoutingTable contacts. timeout contact

      const auto h_depth = h->depth;

      h->~RoutingTable();
      new (h) RoutingTable((ssize_t)depth);

      auto res = update_key(self.rt_reuse, head);
      assertxs(res, h_depth, depth);
      assertxs(*res == h, h_depth, depth);
      // TODO assertxs(res != head, h_depth, depth);??

      return h;
    }
  }

Lbah:
  if (!is_full(self.rt_reuse)) {
    auto result = new RoutingTable((ssize_t)depth);
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
bucket_insert(DHTMetaRoutingTable &self, Bucket &bucket, const Node &c,
              bool eager,
              /*OUT*/ bool &replaced) noexcept {
  replaced = false;
  // TODO binary insert

  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = bucket.contacts[i];
    if (!is_valid(contact)) {
      contact = c;
      bucket.length++;

      return &contact;
    }
  }

  if (eager) {
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &contact = bucket.contacts[i];
      assertx(is_valid(contact));
      if (!is_good(self, contact) || contact.read_only) {
        timeout_unlink_reset(self, contact);

        contact = c;
        assertx(is_valid(contact));

        return &contact;
      }
    }
  }

  return nullptr;
}

static Node *
routing_table_level_insert(DHTMetaRoutingTable &self, RoutingTable &table,
                           const Node &c, bool eager,
                           /*OUT*/ bool &replaced) noexcept {

  for (RoutingTable *it = &table; it; it = it->next) {
    Node *result = bucket_insert(self, it->bucket, c, eager, replaced);
    if (result) {
      return result;
    }
  }

  return nullptr;
}

static Node *
routing_table_level_find_node(RoutingTable &table, const Key &id) noexcept {
  RoutingTable *it = &table;

  while (it) {
    Bucket &bucket = it->bucket;

    // TODO binary search
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

static bool
node_move(DHTMetaRoutingTable &self, Node &subject, Bucket &dest) noexcept {
  assertx(is_valid(subject));
  assertx(subject.timeout_next);
  assertx(subject.timeout_priv);

  Node dummy;
  const bool eager = false;
  bool replaced /*OUT*/ = false;

  auto *nc = bucket_insert(self, dest, dummy, eager, /*OUT*/ replaced);
  assertx(!replaced);
  if (nc) {
    assertx(!nc->timeout_next);
    assertx(!nc->timeout_priv);
    *nc = subject;
    nc->timeout_next = nc->timeout_priv = nullptr;

    timeout::move(*self.tb.timeout, &subject, nc);
    assertx(is_valid(*nc));

    // reset
    subject = Node();
    assertx(!is_valid(subject));
  }

  return nc;
}

static RoutingTable *
split_transfer(DHTMetaRoutingTable &self, RoutingTable *better, Bucket &subject,
               std::size_t level) {
  RoutingTable *better_it = better;

  auto should_transfer = [&self, level](const Node &n) {
    // #if 0
    const bool current_high = bit(n.id, level);
    const bool in_tree_high = bit(self.id, level);
    return current_high == in_tree_high;
    // #endif
    // return prefix_compare(self.id, n.id.id, level + 1);
  };

  assertx(debug_bucket_count(subject) == subject.length);
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &contact = subject.contacts[i];
    if (is_valid(contact)) {
      // printf("- :%s", to_string(contact.id));
      if (should_transfer(contact)) {
        if (!better_it) {
          better_it = alloc_RoutingTable(self, level + 1, AllocType::REC);
          if (!better_it) {
            goto Lreset;
          }
        }

        assertx(!better_it->next);
        while (!node_move(self, /*src*/ contact, better_it->bucket)) {
          assertx(!better_it->next);

          better_it->next = alloc_RoutingTable(self, level + 1, AllocType::REC);
          if (!better_it->next) {
            goto Lreset;
          }
          better_it = better_it->next;
        } // while
        subject.length--;

      } // should_transfer
      else {
        // printf("\n");
      }
    } // if is_valid
  }   // for
  assertx(debug_bucket_count(subject) == subject.length);

  return better_it;
Lreset:
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    if (is_valid(subject.contacts[i])) {
      reset(self, subject.contacts[i]);
      subject.length--;
    }
  }

  assertxs(false, level, length(self.rt_reuse), capacity(self.rt_reuse));
  return better_it;
}

static bool
split(DHTMetaRoutingTable &self, RoutingTable *const split_root) noexcept {
  // debug_print("split-before", self);
  assertx(split_root);
  assertx(!split_root->in_tree);
  /* If there already is a split_root->in_tree node then we already have
   * performed the split and should not get here again.
   */
  // assertxs(split_root->depth == (level + 1), split_root->depth, level);
  const auto target_depth = split_root->depth;

  RoutingTable *better_root = nullptr;
  RoutingTable *better_it = nullptr;

  RoutingTable *split_priv = nullptr;
  RoutingTable *split_it = split_root;

  // printf("id:%s, %zu\n", to_string(self.id), split_root->depth);
  while (split_it) {
    assertx(target_depth == split_it->depth);
    assertx(split_it->in_tree == nullptr);
    Bucket &bucket = split_it->bucket;

    better_it =
        split_transfer(self, /*dest*/ better_it, /*src*/ bucket, target_depth);
    if (!better_root) {
      better_root = better_it;
    }

    if (split_priv) {
      assertx(debug_bucket_count(bucket) == bucket.length);
      /*compact with $split_priv if present*/
      for (std::size_t i = 0; i < Bucket::K; ++i) {
        Node &contact = bucket.contacts[i];
        if (is_valid(contact)) {
          if (node_move(self, contact, split_priv->bucket)) {
            bucket.length--;
            assertx(!is_valid(contact));
          }
        }
      } // for
    }
    assertx(debug_bucket_count(bucket) == bucket.length);

    RoutingTable *const t_next = split_it->next;
    if (bucket.length > 0 || split_it == split_root) {
      split_priv = split_it;
    } else {
      split_it->in_tree = nullptr;
      split_it->next = nullptr;
      dealloc_RoutingTable(self, split_it);
      if (split_priv) {
        split_priv->next = t_next;
      }
    }

    split_it = t_next;
  }
  assertx(better_root);
  assertx(!split_root->in_tree);

  split_root->in_tree = better_root;

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

static void
multiple_closest_nodes(DHTMetaRoutingTable &self, const Key &search,
                       Node **result, std::size_t res_length) noexcept {
  assertx(debug_assert_all(self));
  for (std::size_t i = 0; i < res_length; ++i) {
    assertx(result[i] == nullptr);
  }

  constexpr std::size_t capacity = Bucket::K;

  RoutingTable *raw[capacity] = {nullptr}; // XXX arbitrary
  sp::CircularBuffer<RoutingTable *> best(raw);

  /* Fill buffer */
  RoutingTable *root = self.root;
  std::size_t idx = 0;
  if (root) {
    idx = root->depth;
  }
  const std::size_t max = rank(self.id, search);
Lstart:
  if (root) {
    assertx(root->depth >= 0);
    assertx(idx == (std::size_t)root->depth);
    sp::push_back(best, root);
    debug_bucket_is_valid(root);

    if (idx++ < max) {
      root = root->in_tree;
      goto Lstart;
    }
  }

  std::size_t res_idx = 0;
  auto merge = [&](RoutingTable &table) -> bool {
    for (RoutingTable *it = &table; it; it = it->next) {
      auto &b = it->bucket;

      // printf("[%p]\n", &table);
      assertx(debug_bucket_count(b) == b.length);
      for (std::size_t i = 0; i < capacity && res_idx < res_length; ++i) {
        Node &contact = b.contacts[i];
        if (is_valid(contact)) {

          if (is_good(self, contact)) {
            result[res_idx++] = &contact;
          }
        }
      } // for
    }   // for

    return res_idx == res_length;
  };

  {
    RoutingTable *best_ordered[capacity]{nullptr};
    std::size_t best_length = 0;

    /* Reverse order */
    while (!is_empty(best)) {
      sp::pop_back(best, best_ordered[best_length++]);
    } // while

    for (std::size_t i = 0; i < best_length; ++i) {
      assertx(best_ordered[i]);

      if (merge(*best_ordered[i])) {
        return;
      }
    } // for
  }
} // dht::find_closest_nodes()

//============================================================
bool
is_good(const DHTMetaRoutingTable &dht, const Node &contact) noexcept {
  const Config &config = dht.config;
  // XXX configurable non arbitrary limit?
  if (contact.outstanding > 2) {

    /* Using dht.last_activty to better handle a general outage of network
     * connectivity
     */
    auto resp_timeout = contact.remote_activity + config.refresh_interval;
    // if (resp_timeout > dht.last_activity) {
    if (resp_timeout > dht.now) {
      return false;
    }
  }
  return true;
}

/*public*/
void
multiple_closest(DHTMetaRoutingTable &dht, const NodeId &id, Node **result,
                 std::size_t length) noexcept {
  return multiple_closest_nodes(dht, id.id, result, length);
} // dht::multiple_closest()
  //
void
multiple_closest(DHTMetaRoutingTable &dht, const Key &id, Node **result,
                 std::size_t length) noexcept {
  return multiple_closest_nodes(dht, id, result, length);
} // dht::multiple_closest()

void
multiple_closest(DHTMetaRoutingTable &dht, const Infohash &id, Node **result,
                 std::size_t length) noexcept {
  return multiple_closest_nodes(dht, id.id, result, length);
} // dht::multiple_closest()

Node *
find_node(DHTMetaRoutingTable &self, const NodeId &search) noexcept {
  assertx(debug_assert_all(self));

  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(self, search, inTree, idx);
  if (leaf) {
    return routing_table_level_find_node(*leaf, search.id);
  }

  return nullptr;
} // dht::find_node()

const Bucket *
bucket_for(DHTMetaRoutingTable &dht, const NodeId &id) noexcept {
  bool inTree = false;
  std::size_t idx = 0;

  RoutingTable *leaf = find_closest(dht, id, inTree, idx);
  return leaf ? &leaf->bucket : nullptr;
}

static bool
can_split(const RoutingTable &table, std::size_t idx) {
  const RoutingTable *it = &table;
  while (it) {
    std::size_t bits[2] = {0};

    for (std::size_t i = 0; i < Bucket::K; ++i) {
      const Node &c = it->bucket.contacts[i];
      if (is_valid(c)) {
        // #if 0
        std::size_t bit_idx = bit(c.id, idx) ? 0 : 1;
        bits[bit_idx]++;
        // #endif
      }
    } // for

    if (bits[0] > 0 && bits[1] > 0) {
      return true;
    }

    it = it->next;
  } // while

  return false;
}

#if 0
static void
compact_RoutingTable(dht::DHTMetaRoutingTable &self) {
  while (self.root && length(self.rt_reuse) > self.root_limit &&
         is_empty(*self.root)) {
    assertx(false);
    auto root = self.root;
    RoutingTable *const in_tree = root->in_tree;
    self.root = root->next;
    if (self.root) {
      self.root->in_tree = in_tree;
    } else {
      self.root = in_tree;
    }

    root->next = nullptr;
    root->in_tree = nullptr;
    dealloc_RoutingTable(self, root);
  } // while
}
#endif

Node *
insert(DHTMetaRoutingTable &self, const Node &contact) noexcept {
  if (!is_valid(contact.id)) {
    return nullptr;
  }

  if (self.id == contact.id) {
    return nullptr;
  }

  assertx(debug_assert_all(self));

  bool will_ins = false;

Lstart:
  /* inTree means that contact share the same prefix with self, longer than
   * $bidx.
   */
  bool inTree = false;
  std::size_t xx = 0;

  RoutingTable *const leaf =
      find_closest(self, contact.id, /*OUT*/ inTree, /*OUT*/ xx);
  if (leaf) {
    assertx(self.root);
    auto sp = rank(self.id, contact.id);
    if (sp < self.root->depth) {
      assertxs(!routing_table_level_find_node(*leaf, contact.id.id), sp);
      return nullptr;
    }

    {
      /* Check if already present */
      Node *const existing =
          routing_table_level_find_node(*leaf, contact.id.id);
      if (existing) {
        return existing;
      }
    }

    /* When we are intree meaning we can add another bucket we do not
     * necessarily need to evict a node that might be late responding to pings
     */
    bool eager_merge = /*!inTree;*/ false;
    bool replaced = false;
    assertx(debug_correct_level(self));
    Node *inserted = routing_table_level_insert(self, *leaf, contact,
                                                eager_merge, /*OUT*/ replaced);

    if (inserted) {
      assertxs(prefix_compare(self.id, inserted->id.id, leaf->depth), xx,
               leaf->depth);
      if (self.tb.timeout) {
        timeout::insert_new(*self.tb.timeout, inserted);
      }

      logger::routing::insert(self, *inserted);
      if (!replaced) {
        ++self.total_nodes;
      }
      assertx(debug_correct_level(self));
      assertx((ssize_t)rank(self.id, inserted->id) >= self.root->depth);
    } else {
      assertx(debug_correct_level(self));
      if (!will_ins) {
        debug_print("", self);
        assertx(!will_ins);
      }
      // XXX calc can_split when inserting into bucket
      if (can_split(*leaf, leaf->depth)) {
        // printf("split[depth:%zd]\n", leaf->depth);
        // printf("split[%zu]\n", leaf->depth);
        assertx(!leaf->in_tree);

        // 0: any
        // 1: match
        if (split(self, /*parent*/ leaf)) {
          assertx(leaf->in_tree);
          assertx(debug_correct_level(self));
          // XXX make better
          will_ins = true;
          assertx(debug_assert_all(self));
          goto Lstart;
        }
      } else {
        assertx(debug_correct_level(self));
        auto nxt_depth = leaf->depth;
        auto next = alloc_RoutingTable(self, nxt_depth, AllocType::PLAIN);
        if (next) {
          assertx(next != leaf);
          // printf("next[depth: %zu]\n", nxt_depth);
          assertx(!debug_find(self, next));

          // printf("=====next[%zu]\n", leaf->depth);
          enqueue_level(leaf, next);
          assertx(debug_correct_level(self));
          will_ins = true;
          goto Lstart;
        } else {
          if (!leaf->in_tree) {
            auto depth = rank(self.id, contact.id);
            if (depth > leaf->depth) {
              leaf->in_tree =
                  alloc_RoutingTable(self, leaf->depth + 1, AllocType::REC);
              assertx(leaf->in_tree);
              will_ins = true;
              assertx(debug_assert_all(self));
              goto Lstart;
            }
          }
        }
      }
      auto rnk = rank(self.id, contact.id);
      assertxs((ssize_t)rnk <= self.root->depth, rnk, self.root->depth);

      logger::routing::can_not_insert(self, contact);
    }

    assertx(debug_assert_all(self));
    // compact_RoutingTable(self);

    assertx(debug_assert_all(self));
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
max_routing_nodes(const DHTMetaRoutingTable &) noexcept {
  return std::uint32_t(Bucket::K) * std::uint32_t(sizeof(Key) * 8);
}

std::uint32_t
nodes_good(const DHTMetaRoutingTable &self) noexcept {
  const std::uint32_t result = nodes_total(self) - nodes_bad(self);
  assertx(debug_count_good(self) == result);
  return result;
}

std::uint32_t
nodes_total(const DHTMetaRoutingTable &self) noexcept {
  return self.total_nodes;
}

std::uint32_t
nodes_bad(const DHTMetaRoutingTable &self) noexcept {
  return self.bad_nodes;
}

} // namespace dht
