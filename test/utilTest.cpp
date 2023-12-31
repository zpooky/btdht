#include "util.h"
#include "gtest/gtest.h"
#include <collection/Array.h>
#include <util.h>

TEST(utilTest, test) {
  sp::StaticArray<sp::hasher<Ip>, 2> ip_hashers;
  sp::BloomFilter<Ip, 128> bootstrap_filter(ip_hashers);
  ASSERT_TRUE(insert(ip_hashers, djb_ip));
  ASSERT_TRUE(insert(ip_hashers, fnv_ip));

  for (std::size_t i = 0; i < 128; ++i) {
    Contact c;
    rand_contact(c);
    insert(bootstrap_filter, c.ip);
    ASSERT_TRUE(test(bootstrap_filter, c.ip));
  }
}
