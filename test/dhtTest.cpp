#include "util.h"
#include "gtest/gtest.h"
#include <dht.h>

static void
random_insert(dht::DHT &dht, std::size_t cap) {
  for (std::size_t i = 0; i < cap; ++i) {
    dht::Node n;
    dht::randomize(n.id);

    printf("i%zu\n", i);
    auto *res = dht::insert(dht, n);
    ASSERT_TRUE(res);
  }
}

TEST(dhtTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);
  dht::init(dht);

  random_insert(dht, 8);

  {
    dht::Node self;
    std::memcpy(self.id.id, dht.id.id, sizeof(dht.id.id));
    auto *res = dht::insert(dht, self);
    ASSERT_TRUE(res);
  }
}
