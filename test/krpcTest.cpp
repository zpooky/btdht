#include "util.h"
#include <bencode_offset.h>
#include <bencode_print.h>

template <typename F>
static void
test_request(krpc::ParseContext &ctx, F body) {
  auto f = [&body](krpc::ParseContext &ctx) {

    if (!bencode::d::value(ctx.decoder, "a")) {
      return false;
    }

    return bencode::d::dict(ctx.decoder, body);
  };

  print("request: ", ctx.decoder);
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

  print("response: ", ctx.decoder);
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

    krpc::ParseContext ctx(buff);
    test_request(ctx, [&id](sp::Buffer &d) {
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

    krpc::ParseContext ctx(buff);
    test_response(ctx, [&id](sp::Buffer &d) {
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

    krpc::ParseContext ctx(buff);
    test_request(ctx, [&id](sp::Buffer &p) {
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
      node[i].contact.ip.ipv4 = rand();
      node[i].contact.ip.type = IpType::IPV4;
      node[i].contact.port = rand();
      in[i] = &node[i];
    }

    sp::Buffer buff{b};
    ASSERT_TRUE(
        krpc::response::find_node(buff, t, id, (const dht::Node **)&in, nodes));
    sp::flip(buff);
    // print("find_node_resp:", buff.raw + buff.pos, buff.length);

    krpc::ParseContext ctx(buff);
    test_response(ctx, [&id, &in](sp::Buffer &p) {
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
    ASSERT_TRUE(bencode::d::dict_wildcard(copy));
  }

  krpc::ParseContext ctx(buffer);

  test_response(ctx, [](sp::Buffer &p) {
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
    dht::Token token;
    token.length = 8;
    std::memcpy(token.id, "thetoken", token.length);
    printf("token length: %zu\n", token.length);

    constexpr std::size_t NODE_SIZE = 8;
    dht::Node node[NODE_SIZE];
    const dht::Node *in[NODE_SIZE];
    for (std::size_t i = 0; i < NODE_SIZE; ++i) {
      nodeId(node[i].id);
      node[i].contact.ip.ipv4 = rand();
      node[i].contact.ip.type = IpType::IPV4;
      node[i].contact.port = rand();
      in[i] = &node[i];
    }

    sp::Buffer buff{b};

    dht::Infohash infohash;
    ASSERT_TRUE(krpc::response::get_peers(buff, t, id, token,
                                          (const dht::Node **)&in, NODE_SIZE));
    sp::flip(buff);

    // bencode_print(p);
    // printf("asd\n\n\n\n\n");
    krpc::ParseContext ctx(buff);
    test_response(ctx, [&id, &in, &token](auto &p) { //
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);

      dht::Token oToken;
      if (!bencode::d::pair(p, "token", oToken.id, oToken.length)) {
        printf("failed to parse token\n");
        return false;
      }
      printf("out token length: %zu\n", oToken.length);
      assert_eq(oToken, token);

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
    dht::Token token;
    token.length = 5;
    std::memcpy(token.id, "token", token.length);

    bool implied_port = true;
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::request::announce_peer(buff, t, id, implied_port,
                                             infohash, port, token));
    sp::flip(buff);

    krpc::ParseContext ctx(buff);
    test_request(ctx,
                 [&id, implied_port, infohash, port, token](sp::Buffer &p) {
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
                   dht::Token out_token;
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

    krpc::ParseContext ctx(buff);
    test_response(ctx, [&id](sp::Buffer &p) { //
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

TEST(krpcTest, print_error_debug3) {
  const char *hex =
      "64323a6970363a51e8520d2710313a7264323a696432303ad70694354abe6c1cb7f55962"
      "9c1437773f59726f353a6e6f6465733230383a4ef030fb96998332cec31fadd564083a8e"
      "63af8b18ec2f75cddd4fb6bd1f09cfddbf6a28a0515d8ee93f75dd7b506ffec2f43d3e4b"
      "5bc020e33420b13287aeb947fe638c37300b2b0cf73f0eaa79472218c7625fabf0d86c7a"
      "e31c86b6d0affff889b3daf1dc44984424bb0f0350a0f948bcebada85ff5f4e1d3805062"
      "9d2767b174456463e4cd0a9e605941ffb524159040f68a7c8c519c431e6702454d423030"
      "31434332323535454441585858e46f3a2519d4029a427d6a77c1c5849371e5cff0e145e3"
      "57021ee6e5722cb6976b68313a7069313030303065353a746f6b656e343abb572cc06531"
      "3a74343a6579bd3d313a76343a4c540101313a79313a7265";

  // 64323a6970363a51e8520d2710313a7264323a696432303a47a73cbd4301ea85d192f8aa4fca19191b5d86cd353a6e6f6465733230383a4f0945288d3ea0e937622af619525fd25730abcea772ceb3c8d54e9d686b0f6caa5cb646e687d451de9aee4770c1326525568e114844841539174f1e7d6d1f75613f72641db2662948e071a6ea884a66641a72f726ea9f44ca9bab62c6c7b65e7b684dc442b7e3384d9fc8c13959d28aea5fb24faa352d2b76f5c27897e183c553184cf41dc1a49759ec998f2a997d684c62cac5aead4bad977797654a969dbbe9ebb3a6db3c870c3e99245e0d1c06f163c0121df5ff4f062bd33995c69571b35e3b76fcb3e8b0cffd1558571966e019313a7069313030303065353a746f6b656e343ae26ab6e565313a74343a65670f4c313a76343a4c540101313a79313a7265
  sp::byte b[4096] = {0};
  std::size_t l = std::strlen(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;
  {
    {
      sp::Buffer copy(buffer);
      sp::bencode_print(copy);
    }
  }
}

TEST(krpcTest, print_error_debug2) {

  /*
   * send: 213.109.234.61:31641
   * d
   *  1:t
   *  4:hex[6169B1FA](ai__)
   *  1:y
   *  1:q
   *  1:v
   *  4:sp19
   *  1:q
   *  4:ping
   *  1:a
   *  d
   *   2:id
   *   20:hex[B8EE2EAF70A2485C32789B965CE62753B11CE](_____p_H_2x_____u___)
   *  e
   * e
   */

  /*
   * response:
   * d
   *  2:ip
   *  6:hex[D54182502710](_A_P'_)
   *  1:r
   *  d
   *   2:id
   *   20:hex[DCFDF873EA8C7FE156FB96C5455A43672FA420](_________o_l_EZCg__ )
   *   1:p
   *   i10000e
   *  e
   *  1:t
   *  4:hex[6169B1FA](ai__)
   *  1:v
   *  4:hex[4C5410](LT__)
   *  1:y
   *  1:r
   * e
   */
  const char *hex =
      "64323a6970363ad54182502710313a7264323a696432303adcfdf8073ea8"
      "c7fe156fb96c05455a43672fa420313a706931303030306565313a74343a"
      "6169b1fa313a76343a4c540100313a79313a7265";
  sp::byte b[4096] = {0};
  std::size_t l = std::strlen(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;
  {
    {
      sp::Buffer copy(buffer);
      sp::bencode_print(copy);
    }
  }
}

TEST(krpcTest, print_error_debug) {
  // const char *hex =
  // "64313a656c693230336531383a496e76616c696420606964272076616c"
  //                   "756565313a74343a647269b8313a76343a6c740d60313a79313a6565";

  /*
   * request:
   * send: 95.211.138.82:6881
   * d
   *  1:t
   *  4:hex[65756DE7](eum_)
   *  1:y
   *  1:q
   *  1:v
   *  4:sp19
   *  1:q
   *  9:find_node
   *  1:a
   *  d
   *   2:id
   *   20:hex[B8EE2EAF70A2485C32789B965CE62753B11CE](_____p_H_2x_____u___)
   *   6:target
   *   20:hex[0924BD7EB114A4D089D88739E6419E6DFFDDF](__K___J_____s_d_____)
   *  e
   * e
   */
  /*response:
   * d
   *  1:e
   *  l
   *   i203e
   *   18:Invalid `id' value
   *  e
   *  1:t
   *  4:hex[65756DE7](eum_)
   *  1:v
   *  4:hex[6C74D40](lt__)
   *  1:y
   *  1:e
   * e
   */
  const char *hex = "64313a656c693230336531383a496e76616c696420606964272076616c"
                    "756565313a74343a65756de7313a76343a6c740d40313a79313a6565";

  sp::byte b[4096] = {0};
  std::size_t l = std::strlen(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;
  {
    {
      sp::Buffer copy(buffer);
      sp::bencode_print(copy);
    }
  }
}

TEST(krpcTest, print_find_node_debug) {
  // const char hex[] =
  //     "64313a656c693230336531383a496e76616c696420606964272076616"
  //     "c756565313a74343a6569d4a3313a76343a6c740d60313a79313a6565";
  //
  // const char hex[] =
  // "64313a656c693230336531383a496e76616c696420606964272076616"
  //                    "c756565313a74343a6571541d313a76343a6c740d60313a79313a656"
  //                    "5";
  // const char hex[] =
  // "64313a656c693230336531383a496e76616c696420606964272076616"
  //                    "c756565313a74343a6378d4b3313a76343a6c740d60313a79313a656"
  //                    "5";

  // const char hex[] =
  // "64313a656c693230336531383a496e76616c696420606964272076616"
  //                    "c756565313a74343a616132a8313a76343a6c740d60313a79313a656"
  //                    "5";

  // const char hex[] =
  // "64313a656c693230336531383a496e76616c696420606964272076616"
  //                    "c756565313a74343a6366b21a313a76343a6c740d60313a79313a656"
  //                    "5";
  // const char hex[] =
  //     "64313a7264323a696432303afed70958ef674974863d4235d8cd54665a3b6f63353a6e6f"
  //     "6465733230383af408fd978759a72ef9ed3d2de3e3f9310417d833c39aac1fd75bf47dd3"
  //     "065f89d92e51b29f599bb24a975e32ae548d8759c52fe4f46f4249f1f1bbe9ebb3a6db3c"
  //     "870c3e99245e52251349ec54d4f7817789864e572875a67923585385ce75a8eb916bb8a9"
  //     "c0c491f120b30f615cb0687f7d42a281a596426e5944d9639049801ae9f1f88949f1f1bb"
  //     "e9ebb3a6db3c870c3e99245e52b61f9786936df1e618debd0b4cdbbbd5c717d025aec523"
  //     "1988863ed2c867c8d5f1e07fbcf3b5b84a4f8f6189b7c994db6e08b18167fcca10cae135"
  //     "3a746f6b656e32303ae7c48e3a44f27e575a2a4aba99fda95c0a861b7765313a74343a61"
  //     "61d4a2313a79313a7265";

  // const char hex[] =
  //     "64313a7264323a696432303a84e7bd6c3ed7cfcb9a965827eac0ce27055e6a30353a6e6f"
  //     "6465733230383a832a5b377f5107fea401c110d668d5ebc12239806dfc4b691197830caf"
  //     "57fed8768a7440ff4f686fb3c400e077ea804475431ae1831741d4be8fdf599deb1de469"
  //     "7fbf2a56bc3de7b412a93a9eb88312a1f1f1bbe9ebb3a6db3c870c3e99245e0d90ae5caa"
  //     "c7585783151f4c743f9402d86047a8eadbe68629d6ffd125dc235e1ae183006e6eced869"
  //     "1a87b404fd001f7e9e3676d339bca82b015d3b83067b42d7a28231c5ef1e7e12a878edd2"
  //     "b2372db22132a9d0618313e8f1f1bbe9ebb3a6db3c870c3e99245e0d90b0d68da9532265"
  //     "313a74343a61713365313a79313a7265";

  constexpr std::size_t cap = 17;
  const char *buffer[cap] = {"64313a7264323a696432303afed70958ef674974863d423"
                             "5d8cd54665a3b6f63353a6e6f"
                             "6465733230383af408fd978759a72ef9ed3d2de3e3f9310"
                             "417d833c39aac1fd75bf47dd3"
                             "065f89d92e51b29f599bb24a975e32ae548d8759c52fe4f"
                             "46f4249f1f1bbe9ebb3a6db3c"
                             "870c3e99245e52251349ec54d4f7817789864e572875a67"
                             "923585385ce75a8eb916bb8a9"
                             "c0c491f120b30f615cb0687f7d42a281a596426e5944d96"
                             "39049801ae9f1f88949f1f1bb"
                             "e9ebb3a6db3c870c3e99245e52b61f9786936df1e618deb"
                             "d0b4cdbbbd5c717d025aec523"
                             "1988863ed2c867c8d5f1e07fbcf3b5b84a4f8f6189b7c99"
                             "4db6e08b18167fcca10cae135"
                             "3a746f6b656e32303ae7c48e3a44f27e575a2a4aba99fda"
                             "95c0a861b7765313a74343a61"
                             "61d4a2313a79313a7265",
                             "64313a7264323a696432303aeedb3632c858e108fdd361f"
                             "a140b836e66419d34353a6e6f"
                             "6465733230383acd4e51337145fe74c77c21cfa7f8a81f4"
                             "856102cb89453531ae9cd3797"
                             "1a763ec95f243511191ff9c0486b802bb04f3803acc8d5c"
                             "dbe5561b3959e741d5b628a78"
                             "e5e4340d0a7e3fdc86c1e81ae9cc8e08cd67be675550cec"
                             "b6b9ad66786a5596c42b61681"
                             "d41ae2cc0601d6ae529049f1f1bbe9ebb3a6db3c870ce15"
                             "ced68b0f9bfcd628742a6ba69"
                             "6fc2f7e8d2e94d56307ca8d0f76d4091cb1ae2cdc653333"
                             "f762ea2600bcaf71dd69f1a16"
                             "0dd7e37e1ca7c62277cdfad11a0419454d30ed802f98fb8"
                             "c32cf53e8a758aa93d5c8d565"
                             "313a74343a63622d93313a79313a7265",
                             "64313a7264323a696432303aeedb3632c858e108fdd361f"
                             "a140b836e66419d34353a6e6f"
                             "6465733230383ae295fd0c42d46502dc30446d306f25695"
                             "b22747a760c4c9c1ae1e30d1b"
                             "f1bbe9ebb3a6db3c870c3e99245e0d1c4905a64669bc47e"
                             "310c5ee46b3a6a6ad1e270ff7"
                             "240a019b71337a0587bf7ac8d5e36924c038552434b882d"
                             "4be93c6c6ee28d8615b4c7620"
                             "f8ebc1e2004885d305cb564c146e3a7a3f1319b647077b4"
                             "8afb1aac121e31f8249f1f1bb"
                             "e9ebb3a6db3c870c3e99245e52bdca22014b07e3360ec7d"
                             "79a9484a8ca71d997d503455e"
                             "c8b3d8586b0bc3c491e3032deb78404da9115a3b93d8e9e"
                             "f0d4c91a6de3ed459e71afb65"
                             "313a74343a63718ba5313a79313a7265",
                             "64313a7264323a696432303aeedb3632c858e108fdd361f"
                             "a140b836e66419d34353a6e6f"
                             "6465733230383acd4e51337145fe74c77c21cfa7f8a81f4"
                             "856102cb89453531ae9cd3797"
                             "1a763ec95f243511191ff9c0486b802bb04f3803acc8d5c"
                             "dbe5561b3959e741d5b628a78"
                             "e5e4340d0a7e3fdc86c1e81ae9cc8e08cd67be675550cec"
                             "b6b9ad66786a5596c42b61681"
                             "d41ae2cc0601d6ae529049f1f1bbe9ebb3a6db3c870ce15"
                             "ced68b0f9bfcd628742a6ba69"
                             "6fc2f7e8d2e94d56307ca8d0f76d4091cb1ae2cdc653333"
                             "f762ea2600bcaf71dd69f1a16"
                             "0dd7e37e1ca7c62277cdfad11a0419454d30ed802f98fb8"
                             "c32cf53e8a758aa93d5c8d565"
                             "313a74343a657016b4313a79313a7265",
                             "64313a7264323a696432303aeedb3632c858e108fdd361f"
                             "a140b836e66419d34353a6e6f"
                             "6465733230383ae295fd0c42d46502dc30446d306f25695"
                             "b22747a760c4c9c1ae1e30d1b"
                             "f1bbe9ebb3a6db3c870c3e99245e0d1c4905a64669bc47e"
                             "310c5ee46b3a6a6ad1e270ff7"
                             "240a019b71337a0587bf7ac8d5e36924c038552434b882d"
                             "4be93c6c6ee28d8615b4c7620"
                             "f8ebc1e2004885d305cb564c146e3a7a3f1319b647077b4"
                             "8afb1aac121e31f8249f1f1bb"
                             "e9ebb3a6db3c870c3e99245e52bdca22014b07e3360ec7d"
                             "79a9484a8ca71d997d503455e"
                             "c8b3d8586b0bc3c491e3032deb78404da9115a3b93d8e9e"
                             "f0d4c91a6de3ed459e71afb65"
                             "313a74343a6270ffc7313a79313a7265",
                             "64313a7264323a696432303aeedb3632c858e108fdd361f"
                             "a140b836e66419d34353a6e6f"
                             "6465733230383ae295fd0c42d46502dc30446d306f25695"
                             "b22747a760c4c9c1ae1e30d1b"
                             "f1bbe9ebb3a6db3c870c3e99245e0d1c4905a64669bc47e"
                             "310c5ee46b3a6a6ad1e270ff7"
                             "240a019b71337a0587bf7ac8d5e36924c038552434b882d"
                             "4be93c6c6ee28d8615b4c7620"
                             "f8ebc1e2004885d305cb564c146e3a7a3f1319b647077b4"
                             "8afb1aac121e31f8249f1f1bb"
                             "e9ebb3a6db3c870c3e99245e52bdca22014b07e3360ec7d"
                             "79a9484a8ca71d997d503455e"
                             "c8b3d8586b0bc3c491e3032deb78404da9115a3b93d8e9e"
                             "f0d4c91a6de3ed459e71afb65"
                             "313a74343a617888a2313a79313a7265",
                             "64313a7264323a696432303a3145d473a12bbf1d9f735b2"
                             "a1a65a4348b9f8df4353a6e6f"
                             "6465733230383a48920eff5620a068c98dfe30a749579a4"
                             "39338964a5331bcc49148921e"
                             "f1f1bbe9ebb3a6db3c870c3e99245e0d90b0567ea46cc64"
                             "892075892fefbd633af1bf821"
                             "f1ffd9de46cc1c2e02f6f99f0948922aa8882617a78c326"
                             "a7e91694151f7533598bc1322"
                             "b0073a489209e9ebb3a6db3c870c3e99245e0d1c06b7f14"
                             "e1a82b83217489237a06645de"
                             "75bc7fc83327c0136ae9c831455bebb5ed2d59489272b5c"
                             "bec8e0a1f342a188d6088987c"
                             "886e2f52b3335f5d5c48920b7e244082d990552cf1cb763"
                             "b15abdacba4c5339aa469e165"
                             "313a74343a647520a7313a79313a7265",
                             "64313a7264323a696432303ad3a20bbf5f734bf1b81ea0f"
                             "4d66bcdb35363c4e0353a6e6f"
                             "6465733230383ad01097f812c4f6788527dacbd79cd1e69"
                             "07171125fd35085de39d02318"
                             "6ffab9f23aca84d5f4680063112f5651d079832392c8d5d"
                             "0de587912a633461e9b1a98d3"
                             "9b2eab575e71f53e10d0cf1ae9d0f59a2066b5af7a99682"
                             "72694fe26393d8b2c6679a7ec"
                             "c01ae1d0ae90096ee2cf9c2e7daf2d26d14014e18c88d0d"
                             "57244756c58d1585fa91e7442"
                             "9cb2b0e5ea5f264a9fce6c041657fcade0416ed177689f6"
                             "c8205eca5ce39eb3cdfa5adfe"
                             "02e3dc8e04d546c8d5d11aa30e98788d9f2745f9247fc90"
                             "c4d20d26fefae6acef9420435"
                             "3a746f6b656e32303a2e2a358b99e4121dfa407e9f88bb5"
                             "09dd6fcb5ee65313a74343a61"
                             "6589b0313a79313a7265",
                             "64313a7264323a696432303aed2cbf7e7edabb05901a3ba"
                             "d05b61845fcb317e1353a6e6f"
                             "6465733230383ae0e435e94de4c0f68298c96a86ccb745e"
                             "6e550c3790279ea1ae9e18e4c"
                             "49f8368c8fe570168296069f46696923c873a6007a74b2e"
                             "3e17dfa5d40e2ce841f8758bc"
                             "38379657a86dbb70de4582f0f7e36f99ea1bd534251a101"
                             "fcf1a52cf9f4b21801ed41095"
                             "3073f1e6d75bd807fef4eeec5341bc66c746f64e8dc88c2"
                             "ea6bf05ea63e6a5a7267a84c3"
                             "9e3a14e7f42d9592ad18cc4485538629e85cf1e64d7a9b1"
                             "2db16d0ffc6a283ccd641a019"
                             "a109be3a79075f9f9ae7e1a0798f5af67c8c5b0b4e0dca3"
                             "67ae710a9e6da2618189fbd65"
                             "313a74343a616ef7e2313a79313a7265",
                             "64313a7264323a696432303ae1b7ce32b2cae16468c3040"
                             "0c98789926d00c044353a6e6f"
                             "6465733230383ae0ecf5f9d7a3a7fc1199317f157a7a915"
                             "7a27aa8dadd4aec1ae9e0b0d9"
                             "cd778b6f366a76d9dde976aa275b08c81176f1b5091ae9e"
                             "07632547410d2d20c5292099c"
                             "7b99a39c822a8344af0d291ae9e02ac17f2c37fe22963c9"
                             "46705e4d6504d0f0d2e4283a5"
                             "061ae9e037c6e97465b92a634b8985ce67342099bafbfc5"
                             "fab34751ae9e03d0632f7007d"
                             "c533d50f0a1917db0de1483bd376a359301ae9e03ec4fb0"
                             "fd2b1ce744dad671261fc39b5"
                             "c1f3e15ae65279e028e01685af5f2c5d71ee3a370ed39b2"
                             "8426791791725e4e9f451ae65"
                             "313a74343a6369cd63313a79313a7265",
                             "64313a7264323a696432303a84e7bd6c3ed7cfcb9a96582"
                             "7eac0ce27055e6a30353a6e6f"
                             "6465733230383a832a5b377f5107fea401c110d668d5ebc"
                             "12239806dfc4b691197830caf"
                             "57fed8768a7440ff4f686fb3c400e077ea804475431ae18"
                             "31741d4be8fdf599deb1de469"
                             "7fbf2a56bc3de7b412a93a9eb88312a1f1f1bbe9ebb3a6d"
                             "b3c870c3e99245e0d90ae5caa"
                             "c7585783151f4c743f9402d86047a8eadbe68629d6ffd12"
                             "5dc235e1ae183006e6eced869"
                             "1a87b404fd001f7e9e3676d339bca82b015d3b83067b42d"
                             "7a28231c5ef1e7e12a878edd2"
                             "b2372db22132a9d0618313e8f1f1bbe9ebb3a6db3c870c3"
                             "e99245e0d90b0d68da9532265"
                             "313a74343a61713365313a79313a7265",
                             "64313a7264323a696432303a84e7bd6c3ed7cfcb9a96582"
                             "7eac0ce27055e6a30353a6e6f"
                             "6465733230383a855aac1dbc9df083d5adec489871bad16"
                             "220c42b055e2762b27285a481"
                             "6ffda6cdef16c86e105d606f8fb4a466abbcf4e5bd70a98"
                             "51c6bb3a6db3c870c3e99245e"
                             "0d1c06b747dee9bc02130f83d3856b758ef8658f514db49"
                             "2192d7c6fc3612b7e155ef895"
                             "19dc0085835cac2f167e9c31d437da7d21fad020e7ddfcb"
                             "a12ab2b2a3c8545862111361c"
                             "d518f6986f6509b84778392fee461f89c8caf485693e152"
                             "89858a75873e5e0b13598b005"
                             "d7256201ae46ab1d52852047f1f1bbe9ebb3a6db3c870c3"
                             "e99245e0d9059c921022cbf65"
                             "313a74343a636787cf313a79313a7265",
                             "64313a7264323a696432303a84e7bd6c3ed7cfcb9a96582"
                             "7eac0ce27055e6a30353a6e6f"
                             "6465733230383a81000b602a056ca0f8a49a49011699bf7"
                             "ea20e2655c9fb0b1ae981f710"
                             "3e34760dd3832a4a85115404f7cb359d0cb92dc3bb6d6c8"
                             "0339dc3bcc6f8de6cf6ebc4bf"
                             "2eeef0d26536dd4d99eb58cf7780b6317001de87f323643"
                             "8718f1c2557c0cb94705a78dd"
                             "52232780bdd4a1d94d12f58f4df08a9d6c3ddcd89805f42"
                             "2f4b7fb1fa681d7d5e86839ca"
                             "675f37c436fa2d2e21dc3d71de5fd39b833f0280d164f6c"
                             "89418c442b644cbd06276d12d"
                             "3d52f74faaa431647f80f2f0ec0ac38dbd2ecc5404c7f53"
                             "6841cbec7815f1c02afefbb65"
                             "313a74343a656c4c49313a79313a7265",
                             "64313a7264323a696432303ae48a57a4d567f476a17a915"
                             "80b4f1cd38657d86d353a6e6f"
                             "6465733230383ae3bdada8b226b6ea6dd4d64dc8d884288"
                             "2669b58578b731dd89fe36a93"
                             "3321e751914adf56fecb4b865266f4566d626d1bc4baa7e"
                             "2c2b68e97bbc5ba52145e3e16"
                             "11ca1097e875c33c4647d93a74e28bfd106fdf15588fd86"
                             "00dec8836185dc0325a50ecfe"
                             "03e21ae35eee8710673201b67d3b97f56e88d6110d60249"
                             "1ef46c4c8d5e25cf6f8821872"
                             "1c3a78aebff2f65e58cf2f3caab8499abb7bd3e36f7a10d"
                             "63d7e25186e0ea2bcd6aabc2e"
                             "fe1f308ec4d9ab9f35e25fc314ff60bb565dffca08d8dd8"
                             "e26f47d8ea5d8a4096f1ae165"
                             "313a74343a6171ce5d313a79313a7265",
                             "64313a7264323a696432303ae48a57a4d567f476a17a915"
                             "80b4f1cd38657d86d353a6e6f"
                             "6465733230383ae3bdada8b226b6ea6dd4d64dc8d884288"
                             "2669b58578b731dd89fe36a93"
                             "3321e751914adf56fecb4b865266f4566d626d1bc4baa7e"
                             "2c2b68e97bbc5ba52145e3e16"
                             "11ca1097e875c33c4647d93a74e28bfd106fdf15588fd86"
                             "00dec8836185dc0325a50ecfe"
                             "03e21ae35eee8710673201b67d3b97f56e88d6110d60249"
                             "1ef46c4c8d5e25cf6f8821872"
                             "1c3a78aebff2f65e58cf2f3caab8499abb7bd3e36f7a10d"
                             "63d7e25186e0ea2bcd6aabc2e"
                             "fe1f308ec4d9ab9f35e25fc314ff60bb565dffca08d8dd8"
                             "e26f47d8ea5d8a4096f1ae165"
                             "313a74343a6476afd7313a79313a7265",

                             "64313a7264323a696432303ab306d7322ad9f5597017514"
                             "6ee0f50d4e0a0a205353a6e6f"
                             "6465733230383ab0fc6a95c73e6541e6baa59bc98336633"
                             "dff7409587e22dcdb04b0aaab"
                             "ba65e7535a985640887d438786de6d3abb3ef892364072b"
                             "1c0d84ee5369a3f1d7cd023db"
                             "7b331aa54357025ab961cb59d0b03dd9b3ed890551164dc"
                             "956a2150c9318c1414e82b469"
                             "321ae9b0645cd6ae529049f1f1bbe9ebb3a6db3c870ce1c"
                             "2a58d0a3bfdb15666864df395"
                             "c40c0cc0b7f801aeadd30f685c546df07bc491b0fdfeb29"
                             "f6fc7ae5dad44e738a36396f3"
                             "eb821360573ff6c8d5b1ef060d373a1f56e9b630d59e49a"
                             "528c4b508a94a0c0d34eb3965"
                             "313a74343a6466b5bd313a79313a7265",

                             "64313a7264323a696432303ae7abc9e0349ca76c60f1663"
                             "916b2b7c72c70bd39353a6e6f"
                             "6465733230383ae28fa08798e84467b4e8d039b4ec61a36"
                             "7c0b4e405275341c8d5e3fa8e"
                             "5f8bb766dda2d11fa4e092c8963ab14d065b7991d2c8d5e"
                             "30f966d8132bdfbc7ee19fba3"
                             "01513d6b51fc8d97501ba775fce229bf3ceca742796cbb8"
                             "765b8edf2691ea25fef53d1ef"
                             "586f60e2227cbbe9ebb3a6db3c870c3e99245e0d1c06f15"
                             "2d0bd535d63e2f16dabb02376"
                             "eaf68ebdb7e305de023e81ff10bcbb24da94c5e24b360b0"
                             "82b89f35ff7a9c34ceda10fc8"
                             "539560330fadb21ae1e3bb5f9df428913bbf1eefa050685"
                             "5df7f5271b35ebe204df01365"
                             "313a74343a61636d40313a79313a7265"};

  for (std::size_t i = 0; i < cap; ++i) {
    printf("\n=%zu=====================\n", i);
    const char *hex = buffer[i];
    // TODO fix
    sp::byte b[4096] = {0};
    std::size_t l = std::strlen(hex);
    FromHex(b, hex, l);
    sp::Buffer buffer(b);
    buffer.length = l;
    {
      {
        sp::Buffer copy(buffer);
        sp::bencode_print(copy);
      }

      sp::Buffer copy(buffer);
      krpc::ParseContext ctx(copy);
      test_response(ctx, [](auto &p) {
        bool b_id = false;
        bool b_n = false;
        bool b_p = false;
        bool b_ip = false;
        bool b_t = false;

        dht::NodeId id;
        dht::Token token;

        sp::list<dht::Node> nodes;
        init(nodes, 20);

        std::uint64_t p_param = 0;

      Lstart:
        if (!b_id && bencode::d::pair(p, "id", id.id)) {
          b_id = true;
          goto Lstart;
        }

        if (!b_t && bencode::d::pair(p, "token", token)) {
          b_t = true;
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

        {
          Contact ip;
          if (!b_ip && bencode::d::pair(p, "ip", ip)) {
            b_ip = true;
            goto Lstart;
          }
        }

        if (!(b_id)) {
          return false;
        }

        // handle_response(ctx, id, nodes);
        return true;
      });
    }
  }
}

TEST(krpcTest, debug) {
  const char hex[] = "64313a7264323a696432303a17323a78dac46ada7f7b6d886fb28da"
                     "0cd4ae253323a6970"
                     "343ad5418250353a6e6f6465733230383a3ed1e36fd2a5a2fee56b9"
                     "9825c41a6ebb4c0d1"
                     "da464f699cf82b3ab799e9ebb3a6db3c870c3e99245e0d1c06b7f12"
                     "5bd31a8a0d933eb26"
                     "f5bb58a36d01235d763e593c46189645d5d9e260bb1ae92917114b0"
                     "768f8e04cce67ee57"
                     "4f418c32dd7be3b55fe1f3742c281d88d6ae529049f1f1bbe9ebb3a"
                     "6db3c870ce1ae076c"
                     "c33ae6212f63095b51765f749c287a1076cc560fb608a8576e4dc21"
                     "ae1212f71e9ebb3a6"
                     "db3c870c3e99245e0d1c06b7f15bc43709317d21185aca77c818b20"
                     "0653841da665f6b1e"
                     "751d92ddf1c3bbc8d565313a74343a657545b3313a76343a5554a5b"
                     "1313a79313a7265";

  auto f = [](krpc::ParseContext &pctx) {
    if (std::strcmp(pctx.msg_type, "q") == 0) {
      return false;
    } else if (std::strcmp(pctx.msg_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(pctx.decoder, "r")) {
        return false;
      }

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

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}
