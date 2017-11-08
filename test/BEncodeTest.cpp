#include "gtest/gtest.h"
#include <BEncode.h>
#include <string.h>

using namespace bencode::e;

static bool
EQ(const char *str, const sp::Buffer &b) {
  std::size_t length = strlen(str);
  assert(length == b.pos);
  return memcmp(str, b.raw, length) == 0;
}

TEST(BEncodeTest, integer) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(value(buff, 42));
  ASSERT_TRUE(EQ("i42e", buff));
  buff.pos = 0;
  ASSERT_TRUE(value(buff, -42));
  ASSERT_TRUE(EQ("i-42e", buff));
  buff.pos = 0;
  ASSERT_TRUE(value(buff, 0));
  ASSERT_TRUE(EQ("i0e", buff));
}

TEST(BEncodeTest, str) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(value(buff, "a"));
  ASSERT_TRUE(EQ("1:a", buff));
  buff.pos = 0;
  ASSERT_TRUE(value(buff, "abc"));
  ASSERT_TRUE(EQ("3:abc", buff));
  buff.pos = 0;
  ASSERT_TRUE(value(buff, ""));
  ASSERT_TRUE(EQ("0:", buff));
  buff.pos = 0;
}

TEST(BEncodeTest, lst) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(list(buff, nullptr, [](sp::Buffer &b, void *) { //
    if (!value(b, "a")) {
      return false;
    }
    if (!value(b, 42)) {
      return false;
    }
    if (!value(b, "abc")) {
      return false;
    }
    if (!value(b, -42)) {
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

  ASSERT_TRUE(dict(buff, [](sp::Buffer &b) { //
    if (!value(b, "a")) {
      return false;
    }
    if (!value(b, 42)) {
      return false;
    }
    if (!value(b, "abc")) {
      return false;
    }
    if (!value(b, -42)) {
      return false;
    }
    return true;
  }));
  ASSERT_TRUE(EQ("d1:ai42e3:abci-42ee", buff));
  buff.pos = 0;
}

static bool
lt(const std::uint8_t *a, const std::uint8_t *b) {
  for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i) {
    // printf("%lu|%lu\n", a[i], b[i]);
    if (a[i] < b[i]) {
      return true;
    }
    if (a[i] > b[i]) {
      return false;
    }
  }
  return false;
}

template <std::size_t SIZE>
static void
convert(std::uint64_t i, std::uint8_t (&buf)[SIZE]) {
  static_assert(sizeof(i) == SIZE, "");
  std::size_t shift = (sizeof(i) * 8) - 8;
  for (std::size_t idx = 0; idx < sizeof(i); ++idx) {
    buf[idx] = std::uint8_t(std::uint64_t(i >> shift) & 0xff);
    // printf("(%lu >> %zu) & 0xff=%u\n", i, shift, buf[idx]);
    shift -= 8;
  }
}

TEST(BEncodeTest, generic) {
  std::uint64_t a = 0;
  std::uint64_t b = 0;
  while (b++ < std::uint16_t(~0)) {
    // printf("%lu < %lu\n", a, b);
    std::uint8_t aBuff[sizeof(std::uint64_t)];
    convert(a, aBuff);
    std::uint8_t bBuff[sizeof(std::uint64_t)];
    convert(b, bBuff);
    ASSERT_TRUE(lt(aBuff, bBuff));
    ASSERT_TRUE(a < b);
    a++;
  }
}
