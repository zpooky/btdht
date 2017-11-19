#include "gtest/gtest.h"
#include <bencode.h>
#include <krpc.h>
#include <shared.h>

// using namespace krpc;

// static bool
// EQ(const char *str, const sp::Buffer &b) {
//   std::size_t length = strlen(str);
//   // assert(length == b.pos);
//   return memcmp(str, b.start, length) == 0;
// }

static void
assert_eq(const char *one, const char *two) {
  ASSERT_TRUE(strcmp(one, two) == 0);
}

template <std::size_t SIZE>
static void
assert_eq(const sp::byte (&one)[SIZE], const sp::byte (&two)[SIZE]) {
  ASSERT_TRUE(memcmp(one, two, SIZE) == 0);
}

static void
nodeId(dht::NodeId &id) {
  memset(id.id, 0, sizeof(id.id));
  const char *raw_id = "abcdefghij0123456789";
  memcpy(id.id, raw_id, strlen(raw_id));
}

static void
transaction(krpc::Transaction &t) {
  memcpy(t.id, "aa", 3);
}

TEST(krpcTest, test_ping) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

  {
    ASSERT_TRUE(krpc::request::ping(buff, t, id));
    sp::flip(buff);
    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};
    bencode::d::Decoder p(buff);
    auto f = [&id](bencode::d::Decoder &p, const krpc::Transaction &,
                   const char *, const char *) {

      if (!bencode::d::value(p, "a")) {
        return false;
      }

      return bencode::d::dict(p, [&id](bencode::d::Decoder &p) {
        dht::NodeId sender;
        if (!bencode::d::pair(p, "id", sender.id)) {
          return false;
        }
        assert_eq(sender.id, id.id);

        return true;
      });

      return true;
    };
    ASSERT_TRUE(krpc::d::krpc(p, tOut, msgOut, qOut, f));
    assert_eq(msgOut, "q");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "ping");
  }
  // ASSERT_TRUE( EQ("d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe",
  // buff));

  ASSERT_TRUE(krpc::response::ping(buff, t, id));
}

TEST(krpcTest, test_find_node) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);
  sp::list<dht::Node> list;

  { //
    ASSERT_TRUE(krpc::request::find_node(buff, t, id, id));
    // sp::flip(buff);
    // krpc::Transaction tOut;
    // char msgOut[16] = {0};
    // char qOut[16] = {0};
    // bencode::d::Decoder p(buff);
    // ASSERT_TRUE(krpc::d::krpc(p, tOut, msgOut, qOut,[](){}));
    // ASSERT_TRUE(eq(msgOut, "q"));
    // ASSERT_TRUE(eq(t.id, tOut.id));
  }

  ASSERT_TRUE(krpc::response::find_node(buff, t, id, list));
}

TEST(krpcTest, test_get_peers) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

  dht::Infohash infohash;
  ASSERT_TRUE(krpc::request::get_peers(buff, t, id, infohash));
}

TEST(krpcTest, test_anounce_peer) {
  sp::byte b[256] = {0};
  sp::Buffer buff{b};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

  dht::Infohash infohash;
  ASSERT_TRUE(krpc::request::announce_peer(buff, t, id, true, infohash, 64000,
                                           "token"));
  ASSERT_TRUE(krpc::response::announce_peer(buff, t, id));
}
