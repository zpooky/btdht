#include "krpc.h"
#include "gtest/gtest.h"

// using namespace krpc;

static bool
EQ(const char *str, const sp::Buffer &b) {
  std::size_t length = strlen(str);
  // assert(length == b.pos);
  return memcmp(str, b.start, length) == 0;
}

TEST(krpcTest, test) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  krpc::NodeId id;
  memset(id.id, 0, sizeof(id.id));
  const char *raw_id = "abcdefghij0123456789";
  memcpy(id.id, raw_id, strlen(raw_id));

  ASSERT_TRUE(krpc::request::ping(buff, id));
  printf("%s\n", buff.start);
  ASSERT_TRUE(
      EQ("d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe", buff));
}
