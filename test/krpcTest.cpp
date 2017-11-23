#include "util.h"

// using namespace krpc;

// static bool
// EQ(const char *str, const sp::Buffer &b) {
//   std::size_t length = strlen(str);
//   // assert(length == b.pos);
//   return memcmp(str, b.start, length) == 0;
// }

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
  sp::byte b[2048] = {0};

  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

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
    ASSERT_TRUE(sp::remaining_read(buff) == 0);
    assert_eq(msgOut, "q");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "find_node");
  }

  {
    constexpr std::size_t nodes = 8;
    dht::Node node[nodes];
    const dht::Node *in[nodes];
    for (std::size_t i = 0; i < nodes; ++i) {
      nodeId(node[i].id);
      node[i].peer.ip = rand();
      node[i].peer.port = rand();
      in[i] = &node[i];
    }

    sp::Buffer buff{b};
    ASSERT_TRUE(
        krpc::response::find_node(buff, t, id, (const dht::Node **)&in, nodes));
    sp::flip(buff);
    // print("find_node_resp:", buff.raw + buff.pos, buff.length);

    krpc::Transaction tOut;
    char msgOut[16] = {0};
    char qOut[16] = {0};
    bencode::d::Decoder p(buff);
    test_response(p, tOut, msgOut, qOut, [&id, &in](bencode::d::Decoder &p) {
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);
      //
      sp::list<dht::Node> outList;
      sp::init(outList, 8);
      if (!bencode::d::pair(p, "target", outList)) {
        return false;
      }
      assert_eq(in, outList);
      return true;
    });
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

  {
    dht::Infohash infohash;
    Port port = 64123;
    const char token[] = "token";

    bool implied_port = true;
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
    ASSERT_TRUE(sp::remaining_read(buff) == 0);
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
    ASSERT_TRUE(sp::remaining_read(buff) == 0);
    assert_eq(msgOut, "r");
    assert_eq(t.id, tOut.id);
    assert_eq(qOut, "announce_peer");
  }
}
