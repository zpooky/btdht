#include "util.h"
#include "gtest/gtest.h"
#include <encode_bencode.h>

static bool
EQ(const char *str, const sp::Buffer &b) {
  std::size_t length = strlen(str);
  assert(length == b.pos);
  return memcmp(str, b.raw, length) == 0;
}

TEST(BEncodeTest, integer) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::value(buff, 42));
  ASSERT_TRUE(EQ("i42e", buff));
  buff.pos = 0;
  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::value(buff, -42));
  ASSERT_TRUE(EQ("i-42e", buff));
  buff.pos = 0;
  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::value(buff, 0));
  ASSERT_TRUE(EQ("i0e", buff));
}

TEST(BEncodeTest, str) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::value(buff, "a"));
  ASSERT_TRUE(EQ("1:a", buff));
  buff.pos = 0;
  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::value(buff, "abc"));
  ASSERT_TRUE(EQ("3:abc", buff));
  buff.pos = 0;
  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::value(buff, ""));
  ASSERT_TRUE(EQ("0:", buff));
  buff.pos = 0;
}

TEST(BEncodeTest, lst) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::list(
      buff, nullptr, [](sp::Buffer &b2, void *) { //
        if (!sp::bencode::e<sp::Buffer>::value(b2, "a")) {
          return false;
        }
        if (!sp::bencode::e<sp::Buffer>::value(b2, 42)) {
          return false;
        }
        if (!sp::bencode::e<sp::Buffer>::value(b2, "abc")) {
          return false;
        }
        if (!sp::bencode::e<sp::Buffer>::value(b2, -42)) {
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

  ASSERT_TRUE(sp::bencode::e<sp::Buffer>::dict(buff, [](sp::Buffer &b2) { //
    if (!sp::bencode::e<sp::Buffer>::value(b2, "a")) {
      return false;
    }
    if (!sp::bencode::e<sp::Buffer>::value(b2, 42)) {
      return false;
    }
    if (!sp::bencode::e<sp::Buffer>::value(b2, "abc")) {
      return false;
    }
    if (!sp::bencode::e<sp::Buffer>::value(b2, -42)) {
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

TEST(BEncodeTest, list) {
  sp::byte raw[2048];
  sp::list<dht::Node> list;
  const std::size_t nodes = 8;
  sp::init(list, nodes);

  for (std::size_t i = 0; i < nodes; ++i) {
    dht::Node node;
    nodeId(node.id);
    node.contact.ip.ipv4 = ~Ipv4(0);
    node.contact.ip.type = IpType::IPV4;
    node.contact.port = ~Port(0);

    assert(sp::push_back(list, node));
  }
  {
    sp::Buffer b(raw);
    ASSERT_TRUE(
        sp::bencode::e<sp::Buffer>::pair_id_contact_compact(b, "target", list));
    sp::flip(b);
    // print("list    ", b.raw + b.pos, b.length);

    sp::UinStaticArray<dht::IdContact, 256> outList;
    ASSERT_TRUE(bencode::d::nodes(b, "target", outList));
    assert_eq(list, outList);
  }
}
