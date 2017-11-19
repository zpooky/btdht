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

template <typename T>
static void
assert_eq(const T &first, const T &second) {
  ASSERT_EQ(first, second);
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

template <std::size_t MSG_SIZE, std::size_t QUERY_SIZE, typename F>
static void
test_request(bencode::d::Decoder &p, krpc::Transaction &t,
             char (&msg)[MSG_SIZE], char (&query)[QUERY_SIZE], F body) {
  auto f = [&body](bencode::d::Decoder &p, const krpc::Transaction &,
                   const char *, const char *) {

    if (!bencode::d::value(p, "a")) {
      return false;
    }

    return bencode::d::dict(p, body);
  };
  ASSERT_TRUE(krpc::d::krpc(p, t, msg, query, f));
}

template <std::size_t MSG_SIZE, std::size_t QUERY_SIZE, typename F>
static void
test_response(bencode::d::Decoder &p, krpc::Transaction &t,
              char (&msg)[MSG_SIZE], char (&query)[QUERY_SIZE], F body) {
  auto f = [&body](bencode::d::Decoder &p, const krpc::Transaction &,
                   const char *, const char *) {

    if (!bencode::d::value(p, "r")) {
      return false;
    }

    return bencode::d::dict(p, body);
  };
  ASSERT_TRUE(krpc::d::krpc(p, t, msg, query, f));
}

TEST(krpcTest, test_ping) {
  sp::byte b[256] = {0};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::request::ping(buff, t, id));
    sp::flip(buff);

    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};

    bencode::d::Decoder p(buff);
    test_request(p, tOut, msgOut, qOut, [&id](bencode::d::Decoder &p) {
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      return true;
    });
    //--
    assert_eq(msgOut, "q");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "ping");
  }
  // ASSERT_TRUE( EQ("d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe",
  // buff));
  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::response::ping(buff, t, id));
    sp::flip(buff);

    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};

    bencode::d::Decoder p(buff);
    test_response(p, tOut, msgOut, qOut, [&id](bencode::d::Decoder &p) {
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      return true;
    });
    //--
    assert_eq(msgOut, "r");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "ping");
  }
}

TEST(krpcTest, test_find_node) {
  sp::byte b[256] = {0};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);
  sp::list<dht::Node> list;

  { //
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::request::find_node(buff, t, id, id));
    sp::flip(buff);

    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};
    bencode::d::Decoder p(buff);

    test_request(p, tOut, msgOut, qOut, [&id](bencode::d::Decoder &p) {
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      dht::NodeId target;
      if (!bencode::d::pair(p, "target", target.id)) {
        return false;
      }
      assert_eq(target.id, id.id);
      return true;
    });
    // ASSERT_TRUE(eq(msgOut, "q"));
    // ASSERT_TRUE(eq(t.id, tOut.id));
  }

  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::response::find_node(buff, t, id, list));
  }
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

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

  bool implied_port = true;
  Port port = 64123;
  const char token[] = "token";

  dht::Infohash infohash;
  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::request::announce_peer(buff, t, id, implied_port,
                                             infohash, port, token));
    sp::flip(buff);

    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};

    bencode::d::Decoder p(buff);
    test_request(
        p, tOut, msgOut, qOut,
        [&id, implied_port, infohash, port, token](bencode::d::Decoder &p) {
          dht::NodeId sender;
          if (!bencode::d::pair(p, "id", sender.id)) {
            return false;
          }
          assert_eq(sender.id, id.id);
          //
          bool out_implied_port = false;
          if (!bencode::d::pair(p, "implied_port", out_implied_port)) {
            return false;
          }
          assert_eq(out_implied_port, implied_port);
          //
          dht::Infohash out_infohash;
          if (!bencode::d::pair(p, "info_hash", out_infohash.id)) {
            return false;
          }
          assert_eq(out_infohash.id, infohash.id);
          //
          Port out_port = 0;
          if (!bencode::d::pair(p, "port", out_port)) {
            return false;
          }
          assert_eq(out_port, port);
          //
          char out_token[64] = {0};
          if (!bencode::d::pair(p, "token", out_token)) {
            return false;
          }
          assert_eq(out_token, token);

          return true;
        });
    //--
    assert_eq(msgOut, "q");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "announce_peer");
  }
  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::response::announce_peer(buff, t, id));
    sp::flip(buff);

    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};

    bencode::d::Decoder p(buff);
    test_response(p, tOut, msgOut, qOut, [&id](bencode::d::Decoder &p) { //
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);
      //
      return true;
    });
    assert_eq(msgOut, "r");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "announce_peer");
  }
}
