#include "util.h"

template <typename F>
static void
test_request(krpc::ParseContext &ctx, F body) {
  auto f = [&body](krpc::ParseContext &ctx) {

    if (!bencode::d::value(ctx.decoder, "a")) {
      return false;
    }

    return bencode::d::dict(ctx.decoder, body);
  };

  print("request: ", ctx.decoder.buf);
  ASSERT_TRUE(krpc::d::krpc(ctx, f));
}

template <typename F>
static void
test_response(krpc::ParseContext &ctx, F body) {
  auto f = [&body](krpc::ParseContext &ctx) {

    if (!bencode::d::value(ctx.decoder, "r")) {
      return false;
    }

    return bencode::d::dict(ctx.decoder, body);
  };

  print("response: ", ctx.decoder.buf);
  ASSERT_TRUE(krpc::d::krpc(ctx, f));
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

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_request(ctx, [&id](bencode::d::Decoder &d) {
      dht::NodeId sender;
      if (!bencode::d::pair(d, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      return true;
    });
    //--
    assert_eq(ctx.query, "ping");
    assert_eq(t.id, ctx.tx.id);
    assert_eq(ctx.msg_type, "q");
  }
  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::response::ping(buff, t, id));
    sp::flip(buff);

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_response(ctx, [&id](bencode::d::Decoder &d) {
      dht::NodeId sender;
      if (!bencode::d::pair(d, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      return true;
    });
    //--
    assert_eq(ctx.msg_type, "r");
    assert_eq(t.id, ctx.tx.id);
    ASSERT_EQ(std::size_t(0), std::strlen(ctx.query));
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

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_request(ctx, [&id](bencode::d::Decoder &p) {
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
    assert_eq(ctx.query, "find_node");
    assert_eq(t.id, ctx.tx.id);
    assert_eq(ctx.msg_type, "q");
  }

  {
    constexpr std::size_t nodes = 8;
    dht::Node node[nodes];
    const dht::Node *in[nodes];
    for (std::size_t i = 0; i < nodes; ++i) {
      nodeId(node[i].id);
      node[i].contact.ip = rand();
      node[i].contact.port = rand();
      in[i] = &node[i];
    }

    sp::Buffer buff{b};
    ASSERT_TRUE(
        krpc::response::find_node(buff, t, id, (const dht::Node **)&in, nodes));
    sp::flip(buff);
    // print("find_node_resp:", buff.raw + buff.pos, buff.length);

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_response(ctx, [&id, &in](bencode::d::Decoder &p) {
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);
      //
      sp::list<dht::Node> outList;
      sp::init(outList, 8);
      if (!bencode::d::nodes(p, "target", outList)) {
        return false;
      }
      assert_eq(in, outList);
      return true;
    });
    ASSERT_TRUE(sp::remaining_read(buff) == 0);
    assert_eq(ctx.msg_type, "r");
    assert_eq(t.id, ctx.tx.id);
    ASSERT_EQ(std::size_t(0), std::strlen(ctx.query));
  }
}

TEST(krpcTest, test_find_node2) {
  const char hex[] =
      "64313a7264323a696432303a7ac5c288bd9bd57d84365f95c89d5c623d2f943d353a6e6f"
      "6465733230383afeaa84202c8d4830227a56c7544033e90623e0991fc154144bedfd5b89"
      "203baa56b7e5bc1970f1b82ce5f503f9d80587bc371ae1fb7eeac476f8adb27c65126c20"
      "297a6459bb2f782d20ce180ed8fb94cf49f1f1bbe9ebb3a6db3c870c3e99245e52463149"
      "2ebe18fbaa87e69b6cf058a32389e55596e75df8c6d1ab5eecf7e57f56f4fa0b67662d02"
      "dc5797b3e96028b4dd169b6eee4490f42a1ae9efef9fc925f49425d51833ac6d5988af65"
      "cf5ad1023d4d4ca734e3bc83a1b3c0d01e3cea904ef91f8d69ba32cc2b05c44b0bc8d565"
      "313a74343a6b65ba58313a76343a54583162313a79313a7265";

  sp::byte b[sizeof(hex) * 2] = {0};
  std::size_t l = sizeof(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;

  bencode::d::Decoder p(buffer);
  krpc::ParseContext ctx(p);

  test_response(ctx, [](bencode::d::Decoder &p) {
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }
    //
    sp::list<dht::Node> outList;
    sp::init(outList, 8);
    if (!bencode::d::nodes(p, "target", outList)) {
      return false;
    }
    // assert_eq(in, outList);
    return true;
  });
}

TEST(krpcTest, test_get_peers) {
  sp::byte b[2048] = {0};
  dht::NodeId id;
  nodeId(id);

  krpc::Transaction t;
  transaction(t);

  {
    sp::Buffer buff{b};

    dht::Infohash infohash;
    ASSERT_TRUE(krpc::request::get_peers(buff, t, id, infohash));
    sp::flip(buff);
    // TODO
  }
  /*response Nodes*/
  {
    dht::Token token; // TODO

    constexpr std::size_t NODE_SIZE = 8;
    dht::Node node[NODE_SIZE];
    const dht::Node *in[NODE_SIZE];
    for (std::size_t i = 0; i < NODE_SIZE; ++i) {
      nodeId(node[i].id);
      node[i].contact.ip = rand();
      node[i].contact.port = rand();
      in[i] = &node[i];
    }

    sp::Buffer buff{b};

    dht::Infohash infohash;
    ASSERT_TRUE(krpc::response::get_peers(buff, t, id, token,
                                          (const dht::Node **)&in, NODE_SIZE));
    sp::flip(buff);

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_response(ctx, [&id, &in, &token](auto &p) { //
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      dht::Token oToken;
      if (!bencode::d::pair(p, "token", oToken.id)) {
        return false;
      }
      assert_eq(oToken.id, token.id);

      /*closes K nodes*/
      sp::list<dht::Node> outNodes;
      sp::init(outNodes, 8);
      if (!bencode::d::nodes(p, "nodes", outNodes)) {
        return false;
      }
      assert_eq(in, outNodes);

      return true;
    });
    ASSERT_TRUE(sp::remaining_read(buff) == 0);
    assert_eq(ctx.msg_type, "r");
    assert_eq(t.id, ctx.tx.id);
    ASSERT_EQ(std::size_t(0), std::strlen(ctx.query));
  }
  /*response Peers*/
  {
    sp::Buffer buff{b};

    dht::Token token; // TODO
    dht::Peer peer[16];

    ASSERT_TRUE(krpc::response::get_peers(buff, t, id, token, peer));
    sp::flip(buff);
  }
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

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_request(ctx, [&id, implied_port, infohash, port,
                       token](bencode::d::Decoder &p) {
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
    assert_eq(ctx.msg_type, "q");
    assert_eq(t.id, ctx.tx.id);
    assert_eq(ctx.query, "announce_peer");
  }
  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::response::announce_peer(buff, t, id));
    sp::flip(buff);

    bencode::d::Decoder p(buff);
    krpc::ParseContext ctx(p);
    test_response(ctx, [&id](bencode::d::Decoder &p) { //
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);
      //
      return true;
    });
    ASSERT_TRUE(sp::remaining_read(buff) == 0);
    assert_eq(ctx.msg_type, "r");
    assert_eq(t.id, ctx.tx.id);
    ASSERT_EQ(std::size_t(0), std::strlen(ctx.query));
  }
}
