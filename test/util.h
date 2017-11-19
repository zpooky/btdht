#ifndef SP_MAINLINE_DHT_TEST_UTIL_H
#define SP_MAINLINE_DHT_TEST_UTIL_H

#include "gtest/gtest.h"
#include <bencode.h>
#include <krpc.h>
#include <shared.h>
#include <string.h>

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
