#include "timeout.h"
#include "util.h"
#include "gtest/gtest.h"
#include <dht.h>
#include <list>
#include <set>

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

template <typename T>
static Node *
random_insert(T &added, dht::DHT &dht) {
  dht::Node n;
  dht::randomize(dht, n.id);

  dht::Node *res = dht::insert(dht, n);
  // ASSERT_TRUE(res);
  if (res) {
    assert(res->timeout_next);
    assert(res->timeout_priv);
    insert(added, n.id);
    // printf("i%zu\n", i);
  }

  const dht::Node *find_res = dht::find_contact(dht, n.id);
  if (res) {
    assert(find_res);
    assert(find_res->id == n.id);
  } else {
    assert(!find_res);
  }
  return res;
}

template <typename T>
static void
random_insert(T &added, dht::DHT &dht, std::size_t cap) {
  for (std::size_t i = 0; i < cap; ++i) {
    random_insert(added, dht);
  }
}

static void
assert_empty(const Node &contact) {
  ASSERT_FALSE(bool(contact));
  ASSERT_EQ(contact.timeout_next, nullptr);
  ASSERT_EQ(contact.timeout_priv, nullptr);

  ASSERT_FALSE(dht::is_valid(contact.id));
  ASSERT_EQ(contact.contact.ip.ipv4, Ipv4(0));
  ASSERT_EQ(contact.contact.port, Port(0));

  ASSERT_EQ(contact.request_activity, time_t(0));
  ASSERT_EQ(contact.response_activity, time_t(0));
  ASSERT_EQ(contact.ping_sent, time_t(0));

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
    if (buf[i] != nullptr) {
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
    if (bool(c)) {
      if (c.id == search)
        return &c;
    }
  }

  return nullptr;
}

static void
verify_routing(DHT &dht, std::size_t &nodes, std::size_t &contacts) {
  const NodeId &id = dht.id;
  std::size_t idx = 0;

  RoutingTable *root = dht.root;

Lstart:
  if (root) {
    ++nodes;
    Bucket &bucket = root->bucket;
    printf("%zu.  ", idx);
    print_id(id, idx, "\033[91m");
    for (std::size_t i = 0; i < Bucket::K; ++i) {
      Node &c = bucket.contacts[i];
      if (bool(c)) {
        printf("-");
        print_id(c.id, idx, "\033[92m");
        ++contacts;
        assert_prefix(c.id, id, idx);
      } else {
        assert_empty(c);
      }
    }
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
      Node *res = find(bucket, dht.id);
      ASSERT_FALSE(res == nullptr);
    }
    root = root->in_tree;
    goto Lstart;
  }
}

static std::size_t
count_nodes(const RoutingTable *r) {
  std::size_t result = 0;
  if (r) {
    ++result;
    result += count_nodes(r->in_tree);
  }
  return result;
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
#define assert_present(dht, current)                                           \
  do {                                                                         \
    Node *buff[Bucket::K] = {nullptr};                                         \
    {                                                                          \
      dht::multiple_closest(dht, current, buff);                               \
                                                                               \
      for (std::size_t i = 0; i < 8; ++i) {                                    \
        ASSERT_TRUE(buff[i] != nullptr);                                       \
      }                                                                        \
                                                                               \
      Node *search = find(buff, current);                                      \
      ASSERT_TRUE(search != nullptr);                                          \
      ASSERT_EQ(search->id, current);                                          \
    }                                                                          \
                                                                               \
    {                                                                          \
      dht::Node *res = dht::find_contact(dht, current);                        \
      ASSERT_TRUE(res);                                                        \
      ASSERT_EQ(res->id, current);                                             \
      ASSERT_TRUE(res->timeout_next);                                          \
      ASSERT_TRUE(res->timeout_priv);                                          \
                                                                               \
      timeout::unlink(dht, res);                                               \
      ASSERT_FALSE(res->timeout_next);                                         \
      ASSERT_FALSE(res->timeout_priv);                                         \
                                                                               \
      timeout::append_all(dht, res);                                           \
      ASSERT_TRUE(res->timeout_next);                                          \
      ASSERT_TRUE(res->timeout_priv);                                          \
    }                                                                          \
  } while (0)

static void
insert_self(DHT &dht, std::list<dht::NodeId> &added) {
  dht::Node self;
  std::memcpy(self.id.id, dht.id.id, sizeof(dht.id.id));
  auto *res = dht::insert(dht, self);
  ASSERT_TRUE(res);
  added.push_back(res->id);
}

template <typename T>
static void
assert_count(DHT &dht, T &added) {
  std::size_t v_nodes = 0;
  std::size_t v_contacts = 0;
  verify_routing(dht, v_nodes, v_contacts);

  std::size_t cnt = count_nodes(dht.root);
  printf("added routing nodes %zu\n", cnt);
  printf("added contacts: %zu\n", added.size());

  ASSERT_EQ(v_contacts, added.size());
  ASSERT_EQ(v_nodes, cnt);
}

TEST(dhtTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);
  dht::init(dht);

  std::list<dht::NodeId> added;
  insert_self(dht, added);

  random_insert(added, dht, 1024 * 1024 * 4);

  for (auto &current : added) {
    const dht::Node *res = dht::find_contact(dht, current);
    ASSERT_TRUE(res);
  }

  assert_count(dht, added);

  // printf("==============\n");
  // printf("dht: ");
  // print_id(dht.id, 18, "");
  for (auto &current : added) {
    assert_present(dht, current);
  }

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

static bool
contains(const std::list<dht::NodeId> &n, const NodeId &search) {
  for (const auto &i : n) {
    if (i == search) {
      return true;
    }
  }
  return true;
}

TEST(dhtTest, test_link) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);
  dht::init(dht);

  // TODO set<>
  std::list<dht::NodeId> added;
  random_insert(added, dht, 1024);

  for (auto &current : added) {
    const dht::Node *res = dht::find_contact(dht, current);
    ASSERT_TRUE(res);
  }

  assert_count(dht, added);

  // printf("==============\n");
  // printf("dht: ");
  // print_id(dht.id, 18, "");
  for (auto &current : added) {
    assert_present(dht, current);
  }

  for_each(dht.timeout_node, [&](Node *n) { //
    ASSERT_TRUE(contains(added, n->id));
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
  dht::DHT dht(sock, c);
  dht::init(dht);

  std::list<dht::NodeId> added;
  for (std::size_t i = 1; i <= 50; ++i) {
    // int i = 1;
  Lretry:
    Node *ins = random_insert(added, dht);
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
          ASSERT_TRUE(contains(added, n->id));
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
          ASSERT_TRUE(contains(added, n->id));
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
          ASSERT_TRUE(contains(added, n->id));
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
//   assert_count(dht, added);
//   self_should_be_last(dht);
// }
