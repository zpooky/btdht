#include "timeout.h"
#include "util.h"
#include "gtest/gtest.h"
#include <dht.h>
#include <list>
#include <prng/util.h>
#include <set>
#include <util/assert.h>

using namespace dht;

template <typename T>
static void
insert(std::list<T> &collection, const T &v) noexcept {
  collection.push_back(v);
}

template <typename T>
static void
insert(std::set<T> &collection, const T &v) noexcept {
  collection.insert(v);
}

static Node *
random_insert(dht::DHT &dht) {
  dht::Node n;
  fill(dht.random, n.id.id);

  dht::Node *res = dht::insert(dht, n);
  // ASSERT_TRUE(res);
  if (res) {
    assertx(res->timeout_next);
    assertx(res->timeout_priv);
    // printf("i%zu\n", i);
  }

  const dht::Node *find_res = dht::find_contact(dht, n.id);
  if (res) {
    assertx(find_res);
    assertx(find_res->id == n.id);
  } else {
    assertx(!find_res);
  }

  dht::Node *buf[Bucket::K]{nullptr};
  dht::multiple_closest(dht, n.id, buf);
  dht::Node **it = buf;
  bool found = false;
  // while (*it) {
  for (std::size_t i = 0; i < Bucket::K && it[i]; ++i) {
    if (it[i]->id == n.id) {
      found = true;
      break;
    }
  }

  if (res) {
    // assertx(found);
  } else {
    assertx(!found);
  }

  return res;
}

static void
random_insert(dht::DHT &dht, std::size_t cap) {
  for (std::size_t i = 0; i < cap; ++i) {
    // printf("%zu.\n", i);
    random_insert(dht);
  }
}

static void
assert_empty(const Node &contact) {
  ASSERT_FALSE(is_valid(contact));
  ASSERT_EQ(contact.timeout_next, nullptr);
  ASSERT_EQ(contact.timeout_priv, nullptr);

  ASSERT_FALSE(dht::is_valid(contact.id));
  ASSERT_EQ(contact.contact.ip.ipv4, Ipv4(0));
  ASSERT_EQ(contact.contact.port, Port(0));

  ASSERT_EQ(contact.remote_activity, Timestamp(0));
  ASSERT_EQ(contact.req_sent, Timestamp(0));

  ASSERT_EQ(contact.ping_outstanding, std::uint8_t(0));
  ASSERT_EQ(contact.good, true);
}

#if 0
static void
ads(dht::DHT &dht, dht::RoutingTable *parent, std::size_t it) {
  // printf("%d\n", parent->type);
  ASSERT_TRUE(parent->type == dht::NodeType::LEAF);

  {
    for (std::size_t i = 0; i < dht::Bucket::K; ++i) {
      dht::Node &contact = parent->bucket.contacts[i];
      assert_empty(contact);
    }
  }

  std::size_t idx = 0;
  dht::split(dht, parent, idx);
  ASSERT_TRUE(parent->type == dht::NodeType::NODE);
  ASSERT_TRUE(parent->node.lower != nullptr);
  ASSERT_TRUE(parent->node.higher != nullptr);

  if (it > 0) {
    ads(dht, parent->node.higher, it - 1);

    ads(dht, parent->node.lower, it - 1);
  }
}

TEST(dhtTest, split) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);

  auto parent = new dht::RoutingTable;
  ads(dht, parent, 10);
}

#endif

static void
assert_prefix(const NodeId &id, const NodeId &self, std::size_t idx) {
  for (std::size_t i = 0; i < idx; ++i) {
    bool id_bit = bit(id.id, i);
    bool self_bit = bit(self.id, i);
    ASSERT_EQ(id_bit, self_bit);
  }
}

template <std::size_t size>
static Node *
find(Node *(&buf)[size], const NodeId &search) {
  for (std::size_t i = 0; i < size; ++i) {
    // printf("find: %p\n", buf[i]);
    if (buf[i]) {
      if (buf[i]->id == search) {
        return buf[i];
      }
    }
  }

  return nullptr;
}

static Node *
find(Bucket &bucket, const NodeId &search) {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &c = bucket.contacts[i];
    if (is_valid(c)) {
      if (c.id == search)
        return &c;
    }
  }

  return nullptr;
}

static void
verify_routing(DHT &dht, std::size_t &nodes, std::size_t &contacts) {
  const NodeId &self_id = dht.id;

  assertx(dht.root);
  std::size_t idx = dht.root->depth;

  RoutingTable *root = dht.root;

Lstart:
  if (root) {
    ++nodes;
    printf("%zu.  ", idx);
    print_id(self_id, idx, "\033[91m");
    auto it = root;
    while (it) {
      Bucket &bucket = it->bucket;
      for (std::size_t i = 0; i < Bucket::K; ++i) {
        Node &c = bucket.contacts[i];
        if (is_valid(c)) {
          printf("-");
          print_id(c.id, idx, "\033[92m");
          ++contacts;
          assert_prefix(c.id, self_id, idx);
        } else {
          assert_empty(c);
        }
      }
      it = it->next;
    } // while
    root = root->in_tree;
    ++idx;
    goto Lstart;
  }
}

static void
self_should_be_last(DHT &dht) {
  RoutingTable *root = dht.root;
Lstart:
  if (root) {
    Bucket &bucket = root->bucket;
    if (root->in_tree == nullptr) {
      // selfID should be in the last node routing table
      dht.id.id[19] = ~dht.id.id[19];
      Node *res = find(bucket, dht.id);
      ASSERT_FALSE(res == nullptr);
    }
    root = root->in_tree;
    goto Lstart;
  }
}

// static std::size_t
// equal(const NodeId &id, const Key &cmp) noexcept {
//   std::size_t i = 0;
//   for (; i < NodeId::bits; ++i) {
//     if (bit(id.id, i) != bit(cmp, i)) {
//       return i;
//     }
//   }
//   return i;
// }

// assert_present(DHT &dht, const NodeId &current) {
static void
assert_present(dht::DHT &dht, const Node &current) {
  Node *buff[Bucket::K * 8] = {nullptr};
  {
    dht::multiple_closest(dht, current.id, buff);

    Node *search = find(buff, current.id);
    assertx(search != nullptr);
    ASSERT_EQ(search->id, current.id);
  }

  {
    dht::Node *res = dht::find_contact(dht, current.id);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->id, current.id);
    ASSERT_TRUE(res->timeout_next);
    ASSERT_TRUE(res->timeout_priv);

    timeout::unlink(dht, res);
    ASSERT_FALSE(res->timeout_next);
    ASSERT_FALSE(res->timeout_priv);

    timeout::append_all(dht, res);
    ASSERT_TRUE(res->timeout_next);
    ASSERT_TRUE(res->timeout_priv);
  }
}

#define insert_self(dht)                                                       \
  do {                                                                         \
    dht::Node self;                                                            \
    std::memcpy(self.id.id, dht.id.id, sizeof(dht.id.id));                     \
    dht.id.id[19] = ~dht.id.id[19];                                            \
    auto *res = dht::insert(dht, self);                                        \
    ASSERT_TRUE(res);                                                          \
  } while (0)

static void
assert_count(DHT &dht) {
  std::size_t v_nodes = 0;
  std::size_t v_contacts = 0;
  verify_routing(dht, v_nodes, v_contacts);

  printf("added routing nodes %zu\n", v_nodes);
  printf("added contacts: %zu\n", v_contacts);
}

TEST(dhtTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);
  dht::init(dht);

  insert_self(dht);

  //
  random_insert(dht, 1024 * 1024 * 4);
  assert_count(dht);

  // printf("==============\n");
  // printf("dht: ");
  // print_id(dht.id, 18, "");
  dht::debug_for_each(dht, nullptr,
                      [](void *, dht::DHT &self, const auto &current) {
                        assert_present(self, current);
                      });

  self_should_be_last(dht);
}

template <typename F>
static void
for_each(Node *const start, F f) noexcept {
  Node *it = start;
Lstart:
  if (it) {
    f(it);
    it = it->timeout_next;
    if (it != start) {
      goto Lstart;
    }
  }
}

TEST(dhtTest, test_link) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);
  dht::init(dht);

  random_insert(dht, 1024);

  assert_count(dht);

  // printf("==============\n");
  // printf("dht: ");
  // print_id(dht.id, 18, "");
  dht::debug_for_each(dht, nullptr,
                      [](void *, dht::DHT &self, const auto &current) {
                        assert_present(self, current);
                      });
}

static bool
is_unique(std::list<NodeId> &l) {
  for (auto it = l.begin(); it != l.end(); it++) {
    // printf("NodeId: ");
    // print_hex(*it);
    for (auto lit = it; ++lit != l.end();) {
      // printf("cmp: ");
      // print_hex(*lit);
      if (*it == *lit) {
        return false;
      }
    }
  }
  return true;
}

TEST(dhtTest, test_append) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);
  dht::init(dht);

  for (std::size_t i = 1; i <= 50; ++i) {
    // int i = 1;
  Lretry:
    printf("%zu.\n", i);
    Node *ins = random_insert(dht);
    if (ins == nullptr) {
      goto Lretry;
    }

    {
      dht::Node *res = dht::find_contact(dht, ins->id);
      ASSERT_TRUE(res);
      ASSERT_EQ(res->id, ins->id);
      ASSERT_TRUE(res->timeout_next);
      ASSERT_TRUE(res->timeout_priv);

      {
        std::list<dht::NodeId> unique;

        std::size_t number = 0;
        for_each(dht.timeout_node, [&](Node *n) { //
          ++number;
          insert(unique, n->id);
          // printf("NodeId: ");
          // print_hex(n->id);
        });
        // printf("ASSERT_EQ(number[%zu], i[%zu])\n", number, i);
        ASSERT_EQ(number, i);
        ASSERT_EQ(unique.size(), i);
        ASSERT_TRUE(is_unique(unique));
      }

      timeout::unlink(dht, res);
      ASSERT_FALSE(res->timeout_next);
      ASSERT_FALSE(res->timeout_priv);
      {
        std::list<dht::NodeId> unique;

        std::size_t number = 0;
        for_each(dht.timeout_node, [&](Node *n) { //
          ++number;
          insert(unique, n->id);
        });
        ASSERT_EQ(number, i - 1);
        ASSERT_EQ(unique.size(), i - 1);
        ASSERT_TRUE(is_unique(unique));
      }

      {
        timeout::append_all(dht, res);
        ASSERT_TRUE(res->timeout_next);
        ASSERT_TRUE(res->timeout_priv);
      }

      {
        std::list<dht::NodeId> unique;

        std::size_t number = 0;
        for_each(dht.timeout_node, [&](Node *n) { //
          ++number;
          insert(unique, n->id);
        });
        ASSERT_EQ(number, i);
        ASSERT_EQ(unique.size(), i);
        ASSERT_TRUE(is_unique(unique));
      }
    }
  }
}

TEST(dhtTest, test_node_id_strict) {
  fd sock(-1);
  prng::xorshift32 r(1);
  for (std::size_t i = 0; i < 10000; ++i) {
    Ip ip(i);
    Contact c(ip, 0);
    dht::DHT dht(sock, c, r);
    init(dht);
    ASSERT_TRUE(is_strict(ip, dht.id));
  }
}

TEST(dhtTest, test_node_id_not_strict) {
  fd sock(-1);
  prng::xorshift32 r(1);
  dht::NodeId id;
  for (std::size_t i = 0; i < 500000; ++i) {
    Ip ip(i);
    Contact c(ip, 0);
    fill(r, id.id);
    ASSERT_FALSE(is_strict(ip, id));
  }
}

// TEST(dhtTest, test2) {
//   fd sock(-1);
//   Contact c(0, 0);
//   dht::DHT dht(sock, c);
//   dht::init(dht);
//   std::list<dht::NodeId> added;
//
//   insert_self(dht, added);
//
//   for (std::size_t i = 20; i < NodeId::bits; ++i) {
//     Node test;
//     std::memcpy(test.id.id, dht.id.id, sizeof(test.id.id));
//     test.id.id[i] = !test.id.id[i];
//
//     {
//       const dht::Node *fres = dht::find_contact(dht, test.id);
//       ASSERT_FALSE(fres);
//     }
//
//     Node *res = dht::insert(dht, test);
//     if (res) {
//       // printf("insert(%zu)\n", inserted);
//       ASSERT_TRUE(res);
//       added.push_back(res->id);
//       {
//         const dht::Node *fres = dht::find_contact(dht, res->id);
//         ASSERT_TRUE(fres);
//       }
//     }
//   }
//   // printf("added routing nodes %zu\n", count_nodes(dht.root));
//   // printf("added contacts: %zu\n", added.size());
//
//   for (auto &current : added) {
//     assert_present(dht, current);
//   }
//
//   assert_count(dht);
//   self_should_be_last(dht);
// }
