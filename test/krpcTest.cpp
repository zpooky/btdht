#include "bencode_print.h"
#include "util.h"
#include <bencode_offset.h>

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
      node[i].contact.ipv4 = rand();
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

  // const char hex[] =
  // "64313a7264323a696432303afc102e53210136c224e103a40bb1f7a23"
  //                    "d4e2b06353a6e6f6465733135363a74e20de602321a823b2627374ab1"
  //                    "f7a209d31cb3c39ab5e17442748c1dd22f14188a0b991cc75db1f7a20"
  //                    "4be16d4c39aaca99815748420173a980466225a2b0613b1f7a215ad06"
  //                    "58c39ab302cb9174042cab34df0248286814977fb1f7a20a20162ac39"
  //                    "aaca95800748e196c005d11b61b5d10f22cb1f7a20bc105bd3e4c64eb"
  //                    "787874583d38340638cf06fb2b17bbb1f7a219041974c39aaca996016"
  //                    "5313a74343a696c6723313a79313a7265";
  // const char hex[] =
  //     "64313a7264323a696432303a7ac5c288bd9bd57d84365f95c89d5c623d2f943d353a6e6f"
  //     "6465733230383a765ef7d818bbfdb6d29934e7ed0a05ddb4c6e08b5d2977b83ee876f372"
  //     "72fdbcb8aefa102a4b1ccf25f74430b76ed5884f1b300675c428ac893dfb16cc4a0914fb"
  //     "e140b6e04adac350f5e6dd1ae9725225a36c4258fcc37ae12a791e20ed8081241d5fbdb9"
  //     "bf33df7268a8acdbe4f95db7cb1dabe53e0c422e727acfbb413e466a8b73bc18d3005969"
  //     "ab46f2fe16713690b21899f21b53957d1375f773e1c3940c4234ec10565980092e27aaaa"
  //     "e719a24f1e29f32c5c73e1d09509b83704f298049d6ba4be0e789ac23d53ffb337404f65"
  //     "313a74343a6b659fd7313a76343a54583162313a79313a7265";
  //
  // const char hex[] =
  // "64313a7264323a696432303afcfe3419086428ae3181261083b1f7a23"
  //                    "520068d353a6e6f6465733135363a7456082f0fea0ca33f7d128c4fb1"
  //                    "f7a200a3100f17613b1ad08b74a400063e76056e085507865bb1f7a23"
  //                    "7f00e1fc39aaca9cc2574303d4939bb11ee2225025a1cb1f7a23ef529"
  //                    "e7c39aaca9901b74042cab34df0248286814977fb1f7a20a20162ac39"
  //                    "aaca9580074380fe70ee638050dc024dfd8b1f7a215fc3e96c39aaca9"
  //                    "d34d74142bfb26fd28f101e01689a4b1f7a23f39311ec39aaca9bf2a6"
  //                    "5313a74343a696a015f313a79313a7265";
  const char hex[] = "64313a7264323a696432303af6740b4a1ebc22851a8122215eb1f7a23"
                     "9f437c9353a6e6f6465733135363a9b263668363403df06ef185700b1"
                     "f7a2091f1a13c39ab302b8379bc03ea230f52daf10960a6684b1f7a21"
                     "a5b0149c39aaca9860f9bcc24f0355501360d5914671eb1f7a20a172a"
                     "85c39ab5e1c5e49bbc3f9f168c11dc2593096bdab1f7a208d63a7ad48"
                     "113bca60d9bb230c702fc1e8702b837b3e9b1f7a2092124d1c39ab302"
                     "84599b300fbc247418ff002c399e6db1f7a23ba41b11c39ab5e1c7836"
                     "5313a74343a6972e9dd313a79313a7265";
  sp::byte b[sizeof(hex) * 2] = {0};
  std::size_t l = sizeof(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;
  {
    sp::Buffer copy(buffer);
    bencode::d::Decoder p(copy);
    ASSERT_TRUE(bencode::d::dict_wildcard(p));
  }

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
    if (!bencode::d::nodes(p, "nodes", outList)) {
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
      node[i].contact.ipv4 = rand();
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

TEST(krpcTest, print_debug) {
  // const char hex[] =
  //     "64313a656c693230336531383a496e76616c696420606964272076616"
  //     "c756565313a74343a6569d4a3313a76343a6c740d60313a79313a6565";
  //
  // const char hex[] =
  // "64313a656c693230336531383a496e76616c696420606964272076616"
  //                    "c756565313a74343a6571541d313a76343a6c740d60313a79313a656"
  //                    "5";
  // const char hex[] = "64313a656c693230336531383a496e76616c696420606964272076616"
  //                    "c756565313a74343a6378d4b3313a76343a6c740d60313a79313a656"
  //                    "5";

  const char hex[] =
      "64313a7264323a696432303a84e7bd6c3ed7cfcb9a965827eac0ce27055e6a30353a6e6f"
      "6465733230383a832a5b377f5107fea401c110d668d5ebc12239806dfc4b691197830caf"
      "57fed8768a7440ff4f686fb3c400e077ea804475431ae1831741d4be8fdf599deb1de469"
      "7fbf2a56bc3de7b412a93a9eb88312a1f1f1bbe9ebb3a6db3c870c3e99245e0d90ae5caa"
      "c7585783151f4c743f9402d86047a8eadbe68629d6ffd125dc235e1ae183006e6eced869"
      "1a87b404fd001f7e9e3676d339bca82b015d3b83067b42d7a28231c5ef1e7e12a878edd2"
      "b2372db22132a9d0618313e8f1f1bbe9ebb3a6db3c870c3e99245e0d90b0d68da9532265"
      "313a74343a61713365313a79313a7265";

  // TODO fix
  sp::byte b[sizeof(hex) * 2] = {0};
  std::size_t l = sizeof(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;
  {
    sp::Buffer copy(buffer);
    bencode::d::Decoder p(copy);
    bencode_print(p);
  }
}

TEST(krpcTest, debug) {
  const char hex[] =
      "64313a7264323a696432303a17323a78dac46ada7f7b6d886fb28da0cd4ae253323a6970"
      "343ad5418250353a6e6f6465733230383a3ed1e36fd2a5a2fee56b99825c41a6ebb4c0d1"
      "da464f699cf82b3ab799e9ebb3a6db3c870c3e99245e0d1c06b7f125bd31a8a0d933eb26"
      "f5bb58a36d01235d763e593c46189645d5d9e260bb1ae92917114b0768f8e04cce67ee57"
      "4f418c32dd7be3b55fe1f3742c281d88d6ae529049f1f1bbe9ebb3a6db3c870ce1ae076c"
      "c33ae6212f63095b51765f749c287a1076cc560fb608a8576e4dc21ae1212f71e9ebb3a6"
      "db3c870c3e99245e0d1c06b7f15bc43709317d21185aca77c818b200653841da665f6b1e"
      "751d92ddf1c3bbc8d565313a74343a657545b3313a76343a5554a5b1313a79313a7265";

  auto f = [](krpc::ParseContext &pctx) {
    if (std::strcmp(pctx.msg_type, "q") == 0) {
      return false;
    } else if (std::strcmp(pctx.msg_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(pctx.decoder, "r")) {
        return false;
      }

      printf("asd\n");
      return bencode::d::dict(pctx.decoder, [](auto &p) { //
        bool b_id = false;
        bool b_n = false;
        bool b_p = false;
        bool b_ip = false;

        dht::NodeId id;

        sp::list<dht::Node> nodes;
        init(nodes, 18);
        sp::clear(nodes);

        std::uint64_t p_param = 0;

        sp::byte ip[20];
        std::memset(ip, 0, sizeof(ip));

      Lstart:
        if (!b_id && bencode::d::pair(p, "id", id.id)) {
          b_id = true;
          goto Lstart;
        }

        // optional
        if (!b_n) {
          sp::clear(nodes);
          if (bencode::d::nodes(p, "nodes", nodes)) {
            b_n = true;
            goto Lstart;
          }
        }

        // optional
        if (!b_p && bencode::d::pair(p, "p", p_param)) {
          b_p = true;
          goto Lstart;
        }

        if (!b_ip && bencode::d::pair(p, "ip", ip)) {
          b_ip = true;
          goto Lstart;
        }

        if (!(b_id)) {
          return false;
        }

        return true;
      });
    }

    return false;
  };

  sp::byte b[sizeof(hex) * 2] = {0};
  std::size_t l = sizeof(hex);
  FromHex(b, hex, l);
  sp::Buffer in(b);
  in.length = l;

  bencode::d::Decoder d(in);
  krpc::ParseContext pctx(d);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}
