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
      printf("i%zu\n", i);
    }

    const dht::Node *find_res = dht::find_contact(dht, n.id);
    if (res) {
      ASSERT_TRUE(find_res);
    } else {
      ASSERT_FALSE(find_res);
    }
  }
}

#if 0
static void
ads(dht::DHT &dht, dht::RoutingTable *parent, std::size_t it) {
  // printf("%d\n", parent->type);
  ASSERT_TRUE(parent->type == dht::NodeType::LEAF);

  {
    for (std::size_t i = 0; i < dht::Bucket::K; ++i) {
      dht::Node &contact = parent->bucket.contacts[i];
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


static std::size_t
count_nodes(const RoutingTable *r) {
  std::size_t result = 0;
  if (r) {
    if (r->type == NodeType::LEAF) {
    ++result;
    } else {
      assert(r->type == NodeType::NODE);
      result += count_nodes(r->node.lower);
      result += count_nodes(r->node.higher);
    }
  }
  return result;
}

TEST(dhtTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);
  dht::init(dht);
  std::list<dht::NodeId> added;
  random_insert(added, dht, 8);

  {
    dht::Node self;
    std::memcpy(self.id.id, dht.id.id, sizeof(dht.id.id));
    auto *res = dht::insert(dht, self);
    ASSERT_TRUE(res);
    added.push_back(res->id);
  }

  random_insert(added, dht, 1024 * 1024 * 1);

  printf("\nadded contacts: %zu\n", added.size());
  for (auto &current : added) {
    const dht::Node *res = dht::find_contact(dht, current);
    ASSERT_TRUE(res);
  }
  printf("added routing nodes %zu\n", count_nodes(dht.root));
}
