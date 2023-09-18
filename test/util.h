#ifndef SP_MAINLINE_DHT_TEST_UTIL_H
#define SP_MAINLINE_DHT_TEST_UTIL_H

#include "gtest/gtest.h"
#include <bencode.h>
#include <cstring>
#include <krpc.h>
#include <shared.h>
#include <string/ascii.h>

static inline void
print_hex(const sp::byte *arr, std::size_t length) {
  const std::size_t hex_cap = 4096;
  char hexed[hex_cap + 1] = {0};

  size_t hex_length = 0;
  size_t i = 0;
  while (i < length && hex_length < hex_cap) {
    char buff[128];
    int buffLength = sprintf(buff, "%02x", arr[i++]);
    assertx(buffLength >= 0);
    memcpy(hexed + hex_length, buff, (size_t)buffLength);

    hex_length += (size_t)buffLength;
  }

  if (i == length) {
    printf("%s", hexed);
  } else {
    printf("abbriged[%zu],hex[%zu]:%s", length, i, hexed);
  }
}

template <typename SizeType>
bool
FromHex(sp::byte *theDest, const char *theSource, /*IN/OUT*/ SizeType &i) {
  fprintf(stderr, "%s:%d\n", __func__, (int)'f');
  // TODO ('F'+1)-'0'
  SizeType size = i;
  i = 0;
  const char *it = theSource;
  std::uint8_t lookup['f' + 1];
  lookup[int('0')] = 0x0;
  lookup[int('1')] = 0x1;
  lookup[int('2')] = 0x2;
  lookup[int('3')] = 0x3;
  lookup[int('4')] = 0x4;
  lookup[int('5')] = 0x5;
  lookup[int('6')] = 0x6;
  lookup[int('7')] = 0x7;
  lookup[int('8')] = 0x8;
  lookup[int('9')] = 0x9;

  lookup[int('A')] = 0xA;
  lookup[int('B')] = 0xB;
  lookup[int('C')] = 0xC;
  lookup[int('D')] = 0xD;
  lookup[int('E')] = 0xE;
  lookup[int('F')] = 0xF;

  lookup[int('a')] = 0xA;
  lookup[int('b')] = 0xB;
  lookup[int('c')] = 0xC;
  lookup[int('d')] = 0xD;
  lookup[int('e')] = 0xE;
  lookup[int('f')] = 0xF;

  while (*it) {
    if (i > size) {
      return false;
    }

    char idx = *it++;
    assert((idx >= '0' && idx <= '9') || (idx >= 'a' && idx <= 'f') ||
           (idx >= 'A' && idx <= 'F'));
    sp::byte f = lookup[int(idx)];
    f = sp::byte(f << 4);

    idx = *it++;
    assert((idx >= '0' && idx <= '9') || (idx >= 'a' && idx <= 'f') ||
           (idx >= 'A' && idx <= 'F'));
    sp::byte s = lookup[int(idx)];
    theDest[i++] = f | s;
  }
  return true;
}

inline static void
assert_eq(const char *one, const char *two) {
  if (!one) {
    ASSERT_TRUE(two == nullptr);
  }
  if (!two) {
    ASSERT_TRUE(one == nullptr);
  }
  ASSERT_TRUE(strcmp(one, two) == 0);
}

template <std::size_t SIZE>
inline static void
assert_eq(const sp::byte (&one)[SIZE], const sp::byte (&two)[SIZE]) {
  assertx(memcmp(one, two, SIZE) == 0);
  ASSERT_TRUE(memcmp(one, two, SIZE) == 0);
}

template <typename T>
inline static void
assert_eq(const T &first, const T &second) {
  ASSERT_EQ(first, second);
}

// template <typename T>
// inline static void
// assert_eq(const sp::list<T> &first, const sp::list<T> &second) {
//   ASSERT_EQ(length(first), length(second));
//   // TODO
// }

inline static void
assert_eq(const dht::Node &first, const dht::IdContact &second) {
  assert_eq(first.id.id, second.id.id);
  ASSERT_EQ(first.contact.ip, second.contact.ip);
  ASSERT_EQ(first.contact.port, second.contact.port);
}

inline static void
assert_eq(const sp::list<dht::Node> &first,
          const sp::UinArray<dht::IdContact> &second) {
  ASSERT_EQ(length(first), length(second));

  size_t idx = 0;
  for_each(first, [&](auto &node) { //
    assert_eq(node, second[idx]);
    ++idx;
  });
}

inline static void
assert_eq2(const dht::Node &first, const dht::Node &second) {
  ASSERT_EQ(0, std::memcmp(first.id.id, second.id.id, sizeof(second.id.id)));
  ASSERT_EQ(first.contact, second.contact);
}

template <std::size_t SIZE>
inline static void
assert_eq(const dht::Node *(&first)[SIZE],
          const sp::UinArray<dht::IdContact> &second) {
  ASSERT_EQ(SIZE, length(second));

  for (std::size_t i = 0; i < SIZE; ++i) {
    ASSERT_TRUE(first[i]);
    const auto s = get(second, i);
    ASSERT_TRUE(s);
    assert_eq(*first[i], *s);
  }
}

template <std::size_t SIZE>
inline static void
assert_eq(const dht::Node (&first)[SIZE],
          const sp::UinArray<dht::IdContact> &second) {
  ASSERT_EQ(SIZE, length(second));

  for (std::size_t i = 0; i < SIZE; ++i) {
    ASSERT_TRUE(first[i]);
    const auto s = get(second, i);
    ASSERT_TRUE(s);
    assert_eq(first[i], *s);
  }
}

inline static void
nodeId(dht::NodeId &id) {
  memset(id.id, 0, sizeof(id.id));
  const char *raw_id = "a0b0c0d0e0f0g0SPOOKY";
  memcpy(id.id, raw_id, strlen(raw_id));
}

inline static void
rand_key(dht::Key &id) {
  memset(id, 0, sizeof(id));
  for (std::size_t i = 0; i < sizeof(id) - 1; ++i) {
    id[i] = (sp::byte)rand();
  }
}

inline static void
rand_contact(Contact &c) {
  c.port = (Port)rand();
  c.ip.ipv4 = (Ipv4)rand();
  c.ip.type = IpType::IPV4;
}

inline static void
rand_nodeId(dht::NodeId &id) {
  rand_key(id.id);
}

inline static void
rand_infohash(dht::Infohash &id) {
  rand_key(id.id);
}

static inline void
transaction(krpc::Transaction &t) {
  memcpy(t.id, "aa", 3);
  t.length = 2;
}

static inline void
print(const char *prefix, const sp::byte *b, std::size_t len) noexcept {
  printf("%s", prefix);
  for (std::size_t i = 0; i < len; ++i) {
    if (*b == 0) {
      printf("\\0");
    } else if (!ascii::is_printable(*b)) {
      printf("#");
    } else {
      printf("%c", *b);
    }
    b++;
  }
  printf("\n");
}

static inline void
print(const char *prefix, const sp::Buffer &buffer) noexcept {
  print(prefix, buffer.raw, buffer.length);
}
#endif
