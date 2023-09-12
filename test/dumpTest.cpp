#include <bencode_print.h>
#include <dht.h>
#include <dump.h>
#include <gtest/gtest.h>
#include <prng/util.h>
#include <util.h>

static Contact
rand_contact(prng::xorshift32 &r) {
  return Contact(random(r), Port(random(r)));
}

TEST(dumpTest, test_hex) {
  sp::byte buf[1] = {'\0'};
  dht::print_hex(stdout, buf, sizeof(buf));
  // hex::enc
}

TEST(dumpTest, test) {
  fd sock(-1);
  prng::xorshift32 r(1);
  Contact self = rand_contact(r);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, self, r, now);
  dht::init(dht);

  const char *file = "/tmp/wasd.dump";
  {
    ASSERT_EQ(dht.routing_table.root, nullptr);
    dht.routing_table.root = new dht::RoutingTable(0);
    const std::size_t cap = dht::Bucket::K;
    for (std::size_t i = 0; i < cap; ++i) {
      auto &current = dht.routing_table.root->bucket.contacts[i];
      // TODO rand nodeId
      dht::NodeId id;
      fill(r, id.id);

      current = dht::Node(id, rand_contact(r), Timestamp(0));

      // dummy
      current.timeout_next = &current;
      current.timeout_priv = &current;
    }

    std::size_t nodes = 0;
    for_all_node(dht.routing_table.root, [&nodes](const auto &) {
      ++nodes;
      return true;
    });
    printf("%zu nodes\n", nodes);
  }
  ASSERT_TRUE(sp::dump(dht, file));

  bencode_print_file(file);

  dht::DHT restore_dht(sock, sock, self, r, now);
  ASSERT_TRUE(sp::restore(restore_dht, file));

  ASSERT_EQ(dht.id, restore_dht.id);
  const auto K = dht::Bucket::K;
  ASSERT_EQ(K, length(restore_dht.bootstrap));
  // TODO
}
