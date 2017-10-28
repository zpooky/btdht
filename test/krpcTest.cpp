#include "krpc.h"
#include "gtest/gtest.h"

// using namespace krpc;

// static bool
// EQ(const char *str, const sp::Buffer &b) {
//   std::size_t length = strlen(str);
//   // assert(length == b.pos);
//   return memcmp(str, b.start, length) == 0;
// }

static void
nodeId(krpc::NodeId &id) {
  memset(id.id, 0, sizeof(id.id));
  const char *raw_id = "abcdefghij0123456789";
  memcpy(id.id, raw_id, strlen(raw_id));
}

TEST(krpcTest, test_ping) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  krpc::NodeId id;
  nodeId(id);

  ASSERT_TRUE(krpc::request::ping(buff, id));
  printf("%s\n", buff.start);
  // ASSERT_TRUE( EQ("d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe",
  // buff));

  ASSERT_TRUE(krpc::response::ping(buff, id));
}

TEST(krpcTest, test_find_node) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  krpc::NodeId id;
  nodeId(id);

  const char *target = "target";
  ASSERT_TRUE(krpc::request::find_node(buff, id, target));

  ASSERT_TRUE(krpc::response::find_node(buff, id, target));
}

TEST(krpcTest, test_get_peers) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  krpc::NodeId id;
  nodeId(id);

  const char *infohash = "as";
  ASSERT_TRUE(krpc::request::get_peers(buff, id, infohash));
}

TEST(krpcTest, test_anounce_peer) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  krpc::NodeId id;
  nodeId(id);
  ASSERT_TRUE(
      krpc::request::announce_peer(buff, id, true, "infohash", 64000, "token"));
  ASSERT_TRUE(krpc::response::announce_peer(buff, id));
}
