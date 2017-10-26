#include "BEncode.h"
#include "gtest/gtest.h"
#include <string.h>

using namespace bencode;

static bool
EQ(const char *str, const sp::Buffer &b) {
  std::size_t length = strlen(str);
  assert(length == b.pos);
  return memcmp(str, b.start, length) == 0;
}

TEST(BEncodeTest, integer) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(encode(buff, 42));
  ASSERT_TRUE(EQ("i42e", buff));
  buff.pos = 0;
  ASSERT_TRUE(encode(buff, -42));
  ASSERT_TRUE(EQ("i-42e", buff));
  buff.pos = 0;
  ASSERT_TRUE(encode(buff, 0));
  ASSERT_TRUE(EQ("i0e", buff));
}

TEST(BEncodeTest, str) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(encode(buff, "a"));
  ASSERT_TRUE(EQ("1:a", buff));
  buff.pos = 0;
  ASSERT_TRUE(encode(buff, "abc"));
  ASSERT_TRUE(EQ("3:abc", buff));
  buff.pos = 0;
  ASSERT_TRUE(encode(buff, ""));
  ASSERT_TRUE(EQ("0:", buff));
  buff.pos = 0;
}

TEST(BEncodeTest, lst) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(encodeList(buff, [](sp::Buffer &b) { //
    if (!encode(b, "a")) {
      return false;
    }
    if (!encode(b, 42)) {
      return false;
    }
    if (!encode(b, "abc")) {
      return false;
    }
    if (!encode(b, -42)) {
      return false;
    }
    return true;
  }));
  ASSERT_TRUE(EQ("l1:ai42e3:abci-42ee", buff));
  buff.pos = 0;
}

TEST(BEncodeTest, dict) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(encodeDict(buff, [](sp::Buffer &b) { //
    if (!encode(b, "a")) {
      return false;
    }
    if (!encode(b, 42)) {
      return false;
    }
    if (!encode(b, "abc")) {
      return false;
    }
    if (!encode(b, -42)) {
      return false;
    }
    return true;
  }));
  ASSERT_TRUE(EQ("d1:ai42e3:abci-42ee", buff));
  buff.pos = 0;
}
