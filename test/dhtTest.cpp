#include "util.h"
#include "gtest/gtest.h"
#include <dht.h>
#include <list>

using namespace dht;

static void
random_insert(std::list<dht::NodeId> &added, dht::DHT &dht, std::size_t cap) {
  for (std::size_t i = 0; i < cap; ++i) {
    dht::Node n;
    dht::randomize(n.id);

    dht::Node *res = dht::insert(dht, n);
    // ASSERT_TRUE(res);
    if (res) {
      ASSERT_TRUE(res->timeout_next);
      ASSERT_TRUE(res->timeout_priv);
      added.push_back(n.id);
      // printf("i%zu\n", i);
    }

    const dht::Node *find_res = dht::find_contact(dht, n.id);
    if (res) {
      ASSERT_TRUE(find_res);
    } else {
      ASSERT_FALSE(find_res);
    }
  }
}

static void
assert_empty(const Node &contact) {
  ASSERT_FALSE(bool(contact));
  ASSERT_EQ(contact.timeout_next, nullptr);
  ASSERT_EQ(contact.timeout_priv, nullptr);

  ASSERT_FALSE(dht::is_valid(contact.id));
  ASSERT_EQ(contact.contact.ipv4, 0);
  ASSERT_EQ(contact.contact.port, 0);

  ASSERT_EQ(contact.request_activity, 0);
  ASSERT_EQ(contact.response_activity, 0);
  ASSERT_EQ(contact.ping_sent, 0);

  ASSERT_EQ(contact.ping_outstanding, 0);
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

static bool
bit(const Key &key, std::size_t idx) noexcept {
  // TODO copy
  std::size_t byte = idx / 8;
  std::uint8_t bit = idx % 8;
  std::uint8_t high_bit(1 << 7);
  std::uint8_t bitMask = std::uint8_t(high_bit >> bit);
  return key[byte] & bitMask;
}

static void
assert_prefix(const NodeId &id, const NodeId &self, std::size_t idx) {
  for (std::size_t i = 0; i < idx; ++i) {
    bool id_bit = bit(id.id, i);
    bool self_bit = bit(self.id, i);
    ASSERT_EQ(id_bit, self_bit);
  }
}

static void
print_id(const NodeId &id, std::size_t color, const char *c) {
  for (std::size_t i = 0; i < (sizeof(id.id) * 5); ++i) {
    bool b = bit(id.id, i);
    if (i <= color) {
      printf(c);
    }
    printf("%d", b ? 1 : 0);
    if (i <= color) {
      printf("\033[0m");
    }
  }
  printf("\n");
}

template <std::size_t size>
static Node *
find(Node *(&buf)[size], const NodeId &search) {
  for (std::size_t i = 0; i < size; ++i) {
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
    if (root->in_tree == nullptr) {
      // selfID should be in the last node routing table
      Node *res = find(bucket, dht.id);
      ASSERT_FALSE(res == nullptr);
    }
    root = root->in_tree;
    ++idx;
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

TEST(dhtTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);
  dht::init(dht);
  std::list<dht::NodeId> added;
  {
    {
      dht::Node self;
      std::memcpy(self.id.id, dht.id.id, sizeof(dht.id.id));
      auto *res = dht::insert(dht, self);
      ASSERT_TRUE(res);
      added.push_back(res->id);
    }

    random_insert(added, dht, 1024 * 1024 * 4);

    for (auto &current : added) {
      const dht::Node *res = dht::find_contact(dht, current);
      ASSERT_TRUE(res);
    }

    std::size_t v_nodes = 0;
    std::size_t v_contacts = 0;
    verify_routing(dht, v_nodes, v_contacts);

    std::size_t cnt = count_nodes(dht.root);
    printf("added routing nodes %zu\n", cnt);
    printf("added contacts: %zu\n", added.size());

    ASSERT_EQ(v_contacts, added.size());
    ASSERT_EQ(v_nodes, cnt);
  }
  {
    printf("==============\n");
    std::size_t inserted = 0;
    const std::size_t max(sizeof(dht.id.id) * 8);
    for (std::size_t i = 20; i < max; ++i) {
      Node test;
      test.id = dht.id;
      test.id.id[i] = !test.id.id[i];
      Node *res = dht::insert(dht, test);
      if (res) {
        ++inserted;
        // printf("insert(%zu)\n", inserted);
        ASSERT_TRUE(res);
        added.push_back(res->id);
      }
    }
    printf("added routing nodes %zu\n", count_nodes(dht.root));
    printf("added contacts: %zu\n", added.size());
  }
  //

  for (auto &current : added) {
    Node *buff[8];
    dht::multiple_closest(dht, current, buff);

    for (std::size_t i = 0; i < 8; ++i) {
      ASSERT_TRUE(buff[i] != nullptr);
    }

    Node *search = find(buff, current);
    if (search == nullptr) {
      printf("not found: ");
      print_id(current, 0, "");
    } else {
      printf("found: ");
      print_id(current, 0, "");

      ASSERT_TRUE(search != nullptr);
      ASSERT_EQ(search->id, current);
    }

    {
      const dht::Node *res = dht::find_contact(dht, current);
      ASSERT_TRUE(res);
    }
  }
}
