#ifndef SP_MAINLINE_DHT_TEST_UTIL_H
#define SP_MAINLINE_DHT_TEST_UTIL_H

#include "gtest/gtest.h"
#include <bencode.h>
#include <cstring>
#include <krpc.h>
#include <shared.h>

inline static void
assert_eq(const char *one, const char *two) {
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
  ASSERT_EQ(first.peer.ip, second.peer.ip);
  ASSERT_EQ(first.peer.port, second.peer.port);
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
#endif