#ifndef SP_MAINLINE_DHT_TEST_UTIL_H
#define SP_MAINLINE_DHT_TEST_UTIL_H

#include "gtest/gtest.h"
#include <bencode.h>
#include <cstring>
#include <krpc.h>
#include <shared.h>

static inline void
print_hex(const sp::byte *arr, std::size_t length) {
  const std::size_t hex_cap = 4096;
  char hexed[hex_cap + 1] = {0};

  std::size_t hex_length = 0;
  std::size_t i = 0;
  while (i < length && hex_length < hex_cap) {
    char buff[128];
    std::size_t buffLength = sprintf(buff, "%02x", arr[i++]);
    memcpy(hexed + hex_length, buff, buffLength);

    hex_length += buffLength;
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
  // TODO ('F'+1)-'0'
  SizeType size = i;
  i = 0;
  const char *it = theSource;
  std::uint8_t lookup['f' + 1];
  lookup['0'] = 0x0;
  lookup['1'] = 0x1;
  lookup['2'] = 0x2;
  lookup['3'] = 0x3;
  lookup['4'] = 0x4;
  lookup['5'] = 0x5;
  lookup['6'] = 0x6;
  lookup['7'] = 0x7;
  lookup['8'] = 0x8;
  lookup['9'] = 0x9;
  lookup['a'] = 0xA;
  lookup['b'] = 0xB;
  lookup['c'] = 0xC;
  lookup['d'] = 0xD;
  lookup['e'] = 0xE;
  lookup['f'] = 0xF;

  while (*it) {
    if (i > size) {
      return false;
    }

    char idx = *it++;
    assert(idx >= '0' && idx <= 'f');
    sp::byte f = lookup[idx];
    f = f << 4;

    idx = *it++;
    assert(idx >= '0' && idx <= 'f');
    sp::byte s = lookup[idx];
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
  ASSERT_TRUE(memcmp(one, two, SIZE) == 0);
}

template <typename T>
inline static void
assert_eq(const T &first, const T &second) {
  ASSERT_EQ(first, second);
}

template <typename T>
inline static void
assert_eq(const sp::list<T> &first, const sp::list<T> &second) {
  ASSERT_EQ(first.size, second.size);
  // TODO
}

inline static void
assert_eq2(const dht::Node &first, const dht::Node &second) {
  ASSERT_EQ(0, std::memcmp(first.id.id, second.id.id, sizeof(second.id.id)));
  ASSERT_EQ(first.contact, second.contact);
}

template <std::size_t SIZE>
inline static void
assert_eq(const dht::Node *(&first)[SIZE], const sp::list<dht::Node> &second) {
  ASSERT_EQ(SIZE, second.size);
  for (std::size_t i = 0; i < SIZE; ++i) {
    ASSERT_TRUE(first[i]);
    const dht::Node *s = sp::get(second, i);
    ASSERT_TRUE(s);
    assert_eq2(*first[i], *s);
  }
}

inline static void
nodeId(dht::NodeId &id) {
  memset(id.id, 0, sizeof(id.id));
  const char *raw_id = "abcdefghij0123456789";
  memcpy(id.id, raw_id, strlen(raw_id));
}

static void
transaction(krpc::Transaction &t) {
  memcpy(t.id, "aa", 3);
  t.length = 2;
}

static void
print(const char *prefix, const sp::byte *b, std::size_t len) noexcept {
  printf("%s", prefix);
  for (std::size_t i = 0; i < len; ++i) {
    if (*b == 0) {
      b++;
      printf("0");
    } else {
      printf("%c", *b++);
    }
  }
  printf("\n");
}

static void
print(const char *prefix, const sp::Buffer &buffer) noexcept {
  print(prefix, buffer.raw, buffer.length);
}
#endif
