#include "BEncode.h"
#include "gtest/gtest.h"

using namespace bencode;

TEST(BEncodeTest, dummy) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(encode(buff, 42));
  ASSERT_TRUE(encode(buff, -42));
  ASSERT_TRUE(encode(buff, 0));
  printf("%s\n", b);
}
