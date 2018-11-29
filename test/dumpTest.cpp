#include <dht.h>
#include <dump.h>
#include <gtest/gtest.h>
#include <prng/util.h>

static Contact
rand_contact(prng::xorshift32 &r) {
  return Contact(random(r), random(r));
}

TEST(dumpTest, test) {
  fd sock(-1);
  prng::xorshift32 r(1);
  Contact self = rand_contact(r);
  dht::DHT dht(sock, self, r);
  dht::init(dht);

  const char *file = "/tmp/wasd.dump";
  {
    ASSERT_EQ(dht.root, nullptr);
    dht.root = new dht::RoutingTable;
    const std::size_t cap = dht::Bucket::K;
    for (std::size_t i = 0; i < cap; ++i) {
      // TODO rand nodeId
      dht::NodeId id;
      fill(r, id.id);
      dht.root->bucket.contacts[i] =
          dht::Node(id, rand_contact(r), Timestamp(0));
    }

    std::size_t nodes = 0;
    for_all_node(dht.root, [&nodes](const auto &) {
      ++nodes;
      return true;
    });
    printf("%zu nodes\n", nodes);
  }
  ASSERT_TRUE(sp::dump(dht, file));

  dht::DHT restore_dht(sock, self, r);
  ASSERT_TRUE(sp::restore(restore_dht, file));

  ASSERT_EQ(dht.id, restore_dht.id);
  ASSERT_EQ(dht::Bucket::K, sp::n::length(restore_dht.bootstrap_contacts));
}
