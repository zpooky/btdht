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
  // const char *hex =
  // "64313a7264323a696432303a635b15186f1a13ce53cf3759392b322b73"
  //                   "2a3b78323a6970343a51e8520d65313a74343a616a94a1313a76343a4c"
  //                   "54000f313a79313a7265";

  std::vector<const char *> hex(
      {"64323a6970363a51e8520d2710313a7264323a696432303a4e65e57120db33aab99530a"
       "e"
       "a347dbf43895a0b8353a6e6f6465733230383a4e64d997e5e46a48ad8c1d787c4887473"
       "d"
       "c75324932fcbd01ae94e642b9577d9ade16c785bfe50a8543aca66288c4f6ec8531ae94"
       "e"
       "64127dd96ba8437ed1d3bb402fc9bd05c2ba317896882ee4754e647902d3d175e3fe2b1"
       "6"
       "31b750f6e1f6e96c81d369582e404d4e640c5bc5f1491016b0cd3fd2279a2d600e26675"
       "4"
       "d5245dc8d54e64f849f1f1bbe9ebb3a6db3c870c3e99245e52d58f58dc04114e64c439d"
       "2"
       "161faca24019220228b99f4db1021701e27089a0a54e6423e9e355109ae9fa2a87c12c4"
       "b"
       "9bf6dbdb51c39a4dc4d75b313a7069313030303065353a746f6b656e343ae56308b0363"
       "a"
       "76616c7565736c363a1b22324e61a4363a58cf9bfe1ae1363a721e04120400363a76b0f"
       "5"
       "7e3bcf363a79646e4e333e363ab19e985365a0363abb0d63d6597f363ac55da29d16fc3"
       "6"
       "3aca3e135a3908363adb54d7c941f36565313a74343a6561be98313a76343a4c5401013"
       "1"
       "3a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e659618ce2b8d2a74e2a66"
       "4"
       "7de8739c34e9413c353a6e6f6465733230383a4e64af39e94fed1061368c7a9196db790"
       "9"
       "0f100f5fbe0e765f8e4e649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54"
       "e"
       "64ec00690e1dd14490720849770b1f84aa60e3998562171ae94e64d997e5e46a48ad8c1"
       "d"
       "787c4887473dc75324932fcbd01ae94e642b9577d9ade16c785bfe50a8543aca66288c4"
       "f"
       "6ec8531ae94e640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e6463d52"
       "f"
       "0f55c1ef812712eef0d845c463e29dcb3b16a4dafd4e645449f1f1bbe9ebb3a6db3c870"
       "c"
       "3e99245e52ad31ff63906a353a746f6b656e32303a59406bfa73d6e566a7f0613d594a2"
       "b"
       "d3e7d84cbb363a76616c7565736c363a050e56f53314363acbbe95455edb363ab19e985"
       "3"
       "65a0363a4e49d020f4c0363a7c08df9d41f3363abd7a67c152ae363a5615c7c4d2f6656"
       "5"
       "313a74343a64664281313a76343a5554ad46313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e66e22f7d6d424b47fc12a"
       "5"
       "1a200db817c780ea353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64d0d6ae529049f1f1bbe9ebb3a6db3c870ce1d571f946cdc94"
       "e"
       "640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e647cd6ae529049f1f1b"
       "b"
       "e9ebb3a6db3c870ce1c16fdef96f444e65af19327c323befaa58bd77e90715d26d3ad66"
       "d"
       "81b7efd6b44e65edb8aaf62bb6b809e8866aa0b99f157d6bc24f82f785f43f4e6523094"
       "6"
       "2d4f78be71de2192396ce83eb502e825c9059a17754e65565256018d2faa89acef50218"
       "0"
       "e2693d3a0447e7b5fe4b39353a746f6b656e32303ac823d66799144de263c3ee7972e78"
       "a"
       "3cb92c4c87363a76616c7565736c363a0264402750df363a566169c442b1363a67c03ec"
       "7"
       "d737363a5be24f236c72363a1b22324e61a4363ab19e985365a0363ab03f1456bead656"
       "5"
       "313a74343a6276f283313a76343a5554ad46313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e635e532a5fcefdd866036"
       "0"
       "bc59ee167947962a353a6e6f6465733230383a4e659ba587ce496e4e13795acceded6f2"
       "c"
       "20799db852e6f381634e67144540e9aea3b1118dec04b4fc5f96134e7446a0fd7dc8d54"
       "e"
       "6466a0b40158a30acf8dbcd04cdaea1da07205dce92533c8d54e657a6a4fd1521752c21"
       "8"
       "8e8543615085fd63eec0decd6ac8d54e6734b2a4895b505fdc1d20f06219c48567fc6a4"
       "b"
       "8dd207fcc34e664154c95a23e85f52938e3de30054f0da724854f271ba48564e662f5f1"
       "d"
       "878a270d19f0e73d79361fbad8e08f0530e514e42a4e65fc535aa919e2186530eea1cff"
       "3"
       "7f0bf404b4058f9496bb57353a746f6b656e343a91f3e156363a76616c7565736c363a1"
       "b"
       "22324e61a4363a2e8099f99d6c363a3e1441a47a08363a3e1441a4d359363a5d2307e56"
       "9"
       "c6363a5e31ba8e624b363a7664e74365f9363a9d776979cbaa363aae05937180c5363ab"
       "1"
       "9e985365a0363abc4619d466f1363ac3f2d570498b6565313a74343a6575c746313a763"
       "4"
       "3a4c540010313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e614995684a7dcbe7ae46e"
       "7"
       "81484b5c312a51a8353a6e6f6465733230383a4e6724d7a0b34a67b23a7e6bd212bc671"
       "d"
       "34a0955e3efb15d6874e6423e9e355109ae9fa2a87c12c4b9bf6dbdb51c39a4dc4d75b4"
       "e"
       "649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54e657a6a4fd1521752c21"
       "8"
       "8e8543615085fd63eec0decd6ac8d54e654e2cd9a20cef8eccf34a90d009e1811627df6"
       "7"
       "fcc8702bf74e664154c95a23e85f52938e3de30054f0da724854f271ba48564e66de044"
       "7"
       "b04d9a3416d1287ca720790ab4106085e89593cf024e6731736d6ebcdf0b20de99e5c88"
       "9"
       "1b2159fd7a053b14d01ae1353a746f6b656e343a0e297907363a76616c7565736c363a0"
       "2"
       "27fae442cd363a0264402750df363a1837a20c9d84363a1b059a81042c363a1b22324e6"
       "1"
       "a4363a29b61e0581f3363a2a7312f0743b363a33b36e24b23e363a33b370491efc363a4"
       "c"
       "686aba2c32363a4deeccb3a0d0363a51ce36e7a6e6363a54c6777f040e363a56616d8d0"
       "6"
       "c9363a5774b286cc65363a5d2307e569c6363a5e31ba8e624b363a5e40b31004a1363a6"
       "4"
       "0809e3312a363a66fc0f568cd9363a7664e74365f9363a7c08df9d41f3363a8269f5809"
       "f"
       "24363aae322f229621363ab0e98a10cd9d363ab169c56aa0fe363ab19e985365a0363ab"
       "1"
       "eb1116be8d363ab335af803947363ab3b1be2b9edf363ab3d7086e59c5363abbe2eac36"
       "c"
       "d8363abc06127fd208363abc4619d466f1363abdad4819c1d4363abdd86985d46a363ab"
       "d"
       "def556aff1363abef7c8cd2327363abffd30fa2cb4363ac3f2d570498b363ac5d3d28a8"
       "6"
       "f8363ac94423aaf529363acfcc402e1ec7363adb54d7c941f36565313a74343a6466d58"
       "5"
       "313a76343a4c540010313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e65fc535aa919e2186530e"
       "e"
       "a1cff37f0bf404b4353a6e6f6465733230383a4e64ec00690e1dd14490720849770b1f8"
       "4"
       "aa60e3998562171ae94e6466a0b40158a30acf8dbcd04cdaea1da07205dce92533c8d54"
       "e"
       "6463d52f0f55c1ef812712eef0d845c463e29dcb3b16a4dafd4e6423e9e355109ae9fa2"
       "a"
       "87c12c4b9bf6dbdb51c39a4dc4d75b4e640c5bc5f1491016b0cd3fd2279a2d600e26675"
       "4"
       "d5245dc8d54e64f849f1f1bbe9ebb3a6db3c870c3e99245e52d58f58dc04114e64af39e"
       "9"
       "4fed1061368c7a9196db79090f100f5fbe0e765f8e4e641e9840ae5fcfa989cccb28f41"
       "7"
       "875c5e013c4d2d92905639313a7069313030303065353a746f6b656e343a8677f7e1363"
       "a"
       "76616c7565736c363a0264402750df363a058530295222363a1b22324e61a4363a2e636"
       "d"
       "afc5bd363a4c686aba2c32363a4e49d020f4c0363a4ea58e415691363a53a2ce05cc4a3"
       "6"
       "3a5c821b91dedf363a5e31ba8e624b363a9d32fb5d4e11363ab19e985365a0363ab6fda"
       "3"
       "0c0fb6363abb3ca3fa07e3363abc4619d466f1363abdd8cfa3f190363ac3f2d570498b3"
       "6"
       "3ac8ecf6dcdaf9363acab3198bdfa5363ad422142af0006565313a74343a6575a8a4313"
       "a"
       "76343a4c540100313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e6731736d6ebcdf0b20de9"
       "9"
       "e5c8891b2159fd7a353a6e6f6465733230383a4e654e2cd9a20cef8eccf34a90d009e18"
       "1"
       "1627df67fcc8702bf74e647cd6ae529049f1f1bbe9ebb3a6db3c870ce1c16fdef96f444"
       "e"
       "642b9577d9ade16c785bfe50a8543aca66288c4f6ec8531ae94e65565256018d2faa89a"
       "c"
       "ef502180e2693d3a0447e7b5fe4b394e652309462d4f78be71de2192396ce83eb502e82"
       "5"
       "c9059a17754e640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e6423e9e"
       "3"
       "55109ae9fa2a87c12c4b9bf6dbdb51c39a4dc4d75b4e652722ca84b0d26bd083727ffef"
       "a"
       "4d71e169875bf670580512313a7069313030303065353a746f6b656e343a5238f3ef363"
       "a"
       "76616c7565736c363a0264402750df363a051686beef00363a1b22324e61a4363a52648"
       "3"
       "aa112e363a5d2307e569c6363a5e31ba8e624b363a7664e74365f9363ab19e985365a03"
       "6"
       "3ab2d502706854363abbe2eac36cd8363abc4619d466f1363abd8ca8174261363abdad4"
       "8"
       "19c1d4363abddef556aff1363acbbe95455edb363acfcc402e1ec76565313a74343a657"
       "5"
       "71f1313a76343a4c540102313a79313a7265",
       "64313a7264323a696432303a4e67144540e9aea3b1118dec04b4fc5f96134e74353a6e6"
       "f"
       "6465733230383a4e649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54e642"
       "b"
       "9577d9ade16c785bfe50a8543aca66288c4f6ec8531ae94e640c5bc5f1491016b0cd3fd"
       "2"
       "279a2d600e266754d5245dc8d54e641e49eeb0e78880d7f61ec7b9f7f7994ab073db92a"
       "7"
       "43a83e4e645f62a4dc46494edc755564b9da8ddf054acf4d290b221ae14e6584ee027f1"
       "8"
       "c88ed6cb5f9b3a02075e94464dc30950a29b604e652309462d4f78be71de2192396ce83"
       "e"
       "b502e825c9059a17754e6576aab410d63b8aaaea260d9d3f38a8ff0bcdad5e55f6c4913"
       "5"
       "3a746f6b656e383a25a1b0b3ac5f688a363a76616c7565736c363a4e8270e60001363a0"
       "2"
       "64402750df363a1b22324e61a4363a5e31ba8e624b363a025685ac0001363ab75217740"
       "0"
       "01363a7c08df9d41f3363ab19e985365a0363a293ce83f0001363a51c622480001363ab"
       "a"
       "f1726c57f2363a4ddb0b600001363a5d2307e569c66565313a74343a6466fe8d313a793"
       "1"
       "3a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e652722ca84b0d26bd0837"
       "2"
       "7ffefa4d71e16987353a6e6f6465733230383a4e64bd8be7b454929e083e31f7798e680"
       "9"
       "a01f8e1f94f4bde1594e649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54"
       "e"
       "64e644c8183d8f4a33d011de72d6e97207cfb66d5dd65623844e64c59181379239c0b71"
       "5"
       "68483095b14064628f5f54b65e454f4e642b9577d9ade16c785bfe50a8543aca66288c4"
       "f"
       "6ec8531ae94e6403ef880050163a6599d6516b699a5e5e170fbd36af1e1ae14e6463d52"
       "f"
       "0f55c1ef812712eef0d845c463e29dcb3b16a4dafd4e645449f1f1bbe9ebb3a6db3c870"
       "c"
       "3e99245e52ad31ff63906a353a746f6b656e32303a7881a115d3f38e656b3a45205b99f"
       "c"
       "693669b62b363a76616c7565736c363a5d2307e569c6363ad1ec03aa4cdb363a5981179"
       "9"
       "2b88363a2d73b8b45416363acbbe95455edb363abca9b6ab04cf363a2942ffb4ac41363"
       "a"
       "29af787cf262363a1b22324e61a4363a4e65a1206f2d6565313a74343a65756cb0313a7"
       "6"
       "343a5554ad46313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e65f1977543b44138a2246"
       "6"
       "e908147481b5f076353a6e6f6465733230383a4e6463d52f0f55c1ef812712eef0d845c"
       "4"
       "63e29dcb3b16a4dafd4e6423e9e355109ae9fa2a87c12c4b9bf6dbdb51c39a4dc4d75b4"
       "e"
       "649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54e642b9577d9ade16c785"
       "b"
       "fe50a8543aca66288c4f6ec8531ae94e645449f1f1bbe9ebb3a6db3c870c3e99245e52a"
       "d"
       "31ff63906a4e640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e6403ef8"
       "8"
       "0050163a6599d6516b699a5e5e170fbd36af1e1ae14e64d0d6ae529049f1f1bbe9ebb3a"
       "6"
       "db3c870ce1d571f946cdc9313a7069313030303065353a746f6b656e343ae7cd321e363"
       "a"
       "76616c7565736c363a023287976cdb363a05500d1520c8363a1b22324e61a4363a27362"
       "d"
       "400401363a6d301b482d7b363a7c08df9d41f3363a7c945a592327363ab19e985365a03"
       "6"
       "3abc4619d466f1363abd32cd0fc697363ac34a3351368b363ac55da29d16fc363ad5f70"
       "7"
       "7742976565313a74343a61614df2313a76343a4c540102313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e645f62a4dc46494edc755"
       "5"
       "64b9da8ddf054acf353a6e6f6465733230383a4e64c3a97d5f0fb0a8c529a1c1533413d"
       "3"
       "f98ba8433d455a1ae94e649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54"
       "e"
       "6494a636dfdd47b8dea162b849ffe2dc89bffd6dfc53f048ae4e64db1dfb94525da3ebe"
       "2"
       "8d1b1a1c59757c22eb5f56f8d42e704e64e9b967d13c066d2609b25ea95bd9cf5b27cca"
       "f"
       "2a57c46d174e64ec00690e1dd14490720849770b1f84aa60e3998562171ae94e64ae544"
       "7"
       "692f81f32ab77d1f631b4947c536a8bd817ba552884e64c59181379239c0b7156848309"
       "5"
       "b14064628f5f54b65e454f313a7069313030303065353a746f6b656e343a6a5988f7363"
       "a"
       "76616c7565736c363a0231a9e0fa7e363a4fa8b102b83d363a5d2307e569c6363a73577"
       "9"
       "cc452e363a7c08df9d41f3363ab19e985365a0363ad1ec03aa4cdb6565313a74343a627"
       "2"
       "903c313a76343a4c540101313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e6202c50a403e4c3b96c27"
       "5"
       "e6681463ae9b19cb353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e6463d52f0f55c1ef812712eef0d845c463e29dcb3b16a4dafd4"
       "e"
       "65972f2f83bfd2f56a9fb216ff3240d2d6cf6b4d6cdfba35224e65565256018d2faa89a"
       "c"
       "ef502180e2693d3a0447e7b5fe4b394e66fb0f1231710a49e10f535b58e7edc92086296"
       "1"
       "67f34c23274e662787fbdcf32bf76bf5581949d19a0e79543a5db85155ee224e67960a5"
       "8"
       "5db1fef07be10936b1683a901cff3654f82b133b3d4e675b49f1f1bbe9ebb3a6db3c870"
       "c"
       "3e99245e5249ef869aa5f7353a746f6b656e32303ac16c010b758b5d8ed2046471076b9"
       "3"
       "d4d56777e7363a76616c7565736c363a5d6c63f90940363a02e97d849acb363a5e31ba8"
       "e"
       "624b363a6d5dd536142b363ab19e985365a0363ac0a7174ee507363a296825122761656"
       "5"
       "313a74343a6272df5e313a76343a5554ad47313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e649feb7018c3ae753c83c"
       "2"
       "a912267a88c32a17353a6e6f6465733230383a4e64b1d1d29ace6ffb1e5086d0189e098"
       "7"
       "0022e3010ac62bc4914e648877f02ac68b05b2ee2a4d5f53206ba9f71bbd125a57dc734"
       "e"
       "648aaf55761f997b6bb270d7a513c8d2cf6fb9c308c930bf694e6486f1f4649e8193d78"
       "d"
       "15f0b848169eda9dab4def1a3f79f14e6489344ed2a891d321ca32a806888da874a5b95"
       "f"
       "319757b4e94e6487484864ae25306c8dd95b54e2fbd77bd8bb296f5a3fb04f4e6494a63"
       "6"
       "dfdd47b8dea162b849ffe2dc89bffd6dfc53f048ae4e649b31728e25d25c5907a2aeeac"
       "5"
       "c206190583b067d44121ad313a7069313030303065353a746f6b656e343a86be815f363"
       "a"
       "76616c7565736c363a021f9d86639f363a050cc37c44b0363a055f6a807297363a1805c"
       "0"
       "102d76363a18bb5c4ed9ea363a18e4cf0db493363a25393f1b475d363a298fed1904003"
       "6"
       "3a3ce45576042e363a3d0656cd9c6a363a43cceaa18cc4363a44ad09f33589363a4670a"
       "a"
       "675747363a49417cd7581e363a4a59f106f6d8363a4d8b543be419363a4e3d2c0ef8ab3"
       "6"
       "3a4f254d41db71363a4fb210235058363a4fb570a38eb3363a50fc144281ab363a529a5"
       "7"
       "d8f018363a52efdd55b446363a530b78fa2e97363a546ce165b379363a54c0bb32cf003"
       "6"
       "3a55f15672cfa0363a5626b8dc8541363a5661dbbedc5c363a567940476a93363a57125"
       "4"
       "58362a363a57cf7a236ab0363a5bea8424e326363a5d41301353e8363a5dad423345c33"
       "6"
       "3a5e4e7d6453cd363a5e7c6d7f34cb363a5f41539d9fa0363a5fed6f63ef2a363a63fda"
       "b"
       "eca8d7363a67094bea955f363a6753d6276e29363a675f501f4242363a67dd341e2dab3"
       "6"
       "3a68feca6519ae363a6d31a89b1ae1363a6d51d53d6d04363a6d5d411533fb363a6db15"
       "1"
       "4366db363a6dfc0b5f0d58363a6e2608c40477363a6e27820606d7363a7839146527103"
       "6"
       "3a883e2c04692f363a974ab0cc3569363a9cd14483fceb363aa2ccf812c76e363aa5ff3"
       "e"
       "4d4b14363aa800e20b0400363aaaf6a019f863363aaf9ee13c6749363ab09b9df404003"
       "6"
       "3ab14ade135e2c363ab1c27d35ed4b363ab1ef125b7a5c363ab2a4e0b99aa7363ab3714"
       "e"
       "6b0402363ab9b8574a5573363abb3ca3fa4739363abbace1452e70363abbfac87c33803"
       "6"
       "3abc0248f62e7e363abc92c1bd350c363abd4415822327363abd877f8bea8c363abddff"
       "0"
       "bbdb44363abef8e73de0d6363ac0a67080f33e363ac44b66074f3d363ac530c7be04003"
       "6"
       "3ac8072e8a265f363ac86b2ce743a5363ac98f7c166c2f363accedb39ad6ef363acfcc6"
       "e"
       "89dc5b363ad403c29a5be3363ad45c6bf5c008363ad5bece0ac585363ad98417706d9c6"
       "5"
       "65313a74343a6561282f313a76343a4c540101313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e649feb7018c3ae753c83c"
       "2"
       "a912267a88c32a17353a6e6f6465733230383a4e64b1d1d29ace6ffb1e5086d0189e098"
       "7"
       "0022e3010ac62bc4914e648877f02ac68b05b2ee2a4d5f53206ba9f71bbd125a57dc734"
       "e"
       "648aaf55761f997b6bb270d7a513c8d2cf6fb9c308c930bf694e6486f1f4649e8193d78"
       "d"
       "15f0b848169eda9dab4def1a3f79f14e6489344ed2a891d321ca32a806888da874a5b95"
       "f"
       "319757b4e94e6487484864ae25306c8dd95b54e2fbd77bd8bb296f5a3fb04f4e6494a63"
       "6"
       "dfdd47b8dea162b849ffe2dc89bffd6dfc53f048ae4e649b31728e25d25c5907a2aeeac"
       "5"
       "c206190583b067d44121ad313a7069313030303065353a746f6b656e343a86be815f363"
       "a"
       "76616c7565736c363a021f9d86639f363a050cc37c44b0363a055f6a807297363a1805c"
       "0"
       "102d76363a18bb5c4ed9ea363a18e4cf0db493363a25393f1b475d363a298fed1904003"
       "6"
       "3a3ce45576042e363a3d0656cd9c6a363a43cceaa18cc4363a44ad09f33589363a4670a"
       "a"
       "675747363a49417cd7581e363a4a59f106f6d8363a4d8b543be419363a4e3d2c0ef8ab3"
       "6"
       "3a4f254d41db71363a4fb210235058363a4fb570a38eb3363a50fc144281ab363a529a5"
       "7"
       "d8f018363a52efdd55b446363a530b78fa2e97363a546ce165b379363a54c0bb32cf003"
       "6"
       "3a55f15672cfa0363a5626b8dc8541363a5661dbbedc5c363a567940476a93363a57125"
       "4"
       "58362a363a57cf7a236ab0363a5bea8424e326363a5d41301353e8363a5dad423345c33"
       "6"
       "3a5e4e7d6453cd363a5e7c6d7f34cb363a5f41539d9fa0363a5fed6f63ef2a363a63fda"
       "b"
       "eca8d7363a67094bea955f363a6753d6276e29363a675f501f4242363a67dd341e2dab3"
       "6"
       "3a68feca6519ae363a6d31a89b1ae1363a6d51d53d6d04363a6d5d411533fb363a6db15"
       "1"
       "4366db363a6dfc0b5f0d58363a6e2608c40477363a6e27820606d7363a7839146527103"
       "6"
       "3a883e2c04692f363a974ab0cc3569363a9cd14483fceb363aa2ccf812c76e363aa5ff3"
       "e"
       "4d4b14363aa800e20b0400363aaaf6a019f863363aaf9ee13c6749363ab09b9df404003"
       "6"
       "3ab14ade135e2c363ab1c27d35ed4b363ab1ef125b7a5c363ab2a4e0b99aa7363ab3714"
       "e"
       "6b0402363ab9b8574a5573363abb3ca3fa4739363abbace1452e70363abbfac87c33803"
       "6"
       "3abc0248f62e7e363abc92c1bd350c363abd4415822327363abd877f8bea8c363abddff"
       "0"
       "bbdb44363abef8e73de0d6363ac0a67080f33e363ac44b66074f3d363ac530c7be04003"
       "6"
       "3ac8072e8a265f363ac86b2ce743a5363ac98f7c166c2f363accedb39ad6ef363acfcc6"
       "e"
       "89dc5b363ad403c29a5be3363ad45c6bf5c008363ad5bece0ac585363ad98417706d9c6"
       "5"
       "65313a74343a6176fa51313a76343a4c540101313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e662787fbdcf32bf76bf55"
       "8"
       "1949d19a0e79543a353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64c3a97d5f0fb0a8c529a1c1533413d3f98ba8433d455a1ae94"
       "e"
       "642b9577d9ade16c785bfe50a8543aca66288c4f6ec8531ae94e645449f1f1bbe9ebb3a"
       "6"
       "db3c870c3e99245e52ad31ff63906a4e65a7ee0fa2f40ff553fdde0dc1d2475d6b42eb4"
       "d"
       "7983aee5a64e65cc9d2ce22d471dfd5e0d305792eed602abc2b0e2ab96bf694e6511862"
       "7"
       "4b7818f2a47a27c376761a75fc4cff4fb32c4c564f4e65565256018d2faa89acef50218"
       "0"
       "e2693d3a0447e7b5fe4b39353a746f6b656e32303ad5e8807cea547f587597acdc445c4"
       "0"
       "0c436a0102363a76616c7565736c363a0264402750df363a4c686aba2c32363acbbe954"
       "5"
       "5edb6565313a74343a6272b534313a76343a5554ad46313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e606549f1f1bbe9ebb3a6d"
       "b"
       "3c870c3e99245e52353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e645449f1f1bbe9ebb3a6db3c870c3e99245e52ad31ff63906a4"
       "e"
       "6591524d2374258eeae12222a375c3aa8a989b4d6cdfbaa4544e657249fa6c7762ea25e"
       "9"
       "6fdb559add4effccca5d97a6f22de54e66b80c2d6d76ec07b171ea831ae0dc32c20a654"
       "d"
       "8a99fafddb4e664154c95a23e85f52938e3de30054f0da724854f271ba48564e67f649f"
       "1"
       "f1bbe9ebb3a6db3c870c3e99245e52bd1f0b47a6744e675b49f1f1bbe9ebb3a6db3c870"
       "c"
       "3e99245e5249ef869aa5f7353a746f6b656e32303a34efb707f3217230c7981375c1e66"
       "4"
       "1df8ff6aaf363a76616c7565736c363a5e31ba8e624b363a7defa60048be363ab19e985"
       "3"
       "65a0363a1b22324e61a4363a42fda81d6486363a5ecc691d0400363a56635b22647c363"
       "a"
       "ba708d9d324d363a29af787cf2626565313a74343a61762c51313a76343a5554ad46313"
       "a"
       "79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e675b49f1f1bbe9ebb3a6d"
       "b"
       "3c870c3e99245e52353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64c3a97d5f0fb0a8c529a1c1533413d3f98ba8433d455a1ae94"
       "e"
       "6423e9e355109ae9fa2a87c12c4b9bf6dbdb51c39a4dc4d75b4e6463d52f0f55c1ef812"
       "7"
       "12eef0d845c463e29dcb3b16a4dafd4e659ba587ce496e4e13795acceded6f2c20799db"
       "8"
       "52e6f381634e65da44877e54fb78fd18cefb672b44421fe95dddc0b2494e984e6523094"
       "6"
       "2d4f78be71de2192396ce83eb502e825c9059a17754e6576aab410d63b8aaaea260d9d3"
       "f"
       "38a8ff0bcdad5e55f6c491353a746f6b656e32303a4e101271c0074f8f24df8ca6bd636"
       "7"
       "e8041f44db363a76616c7565736c363ab19e985365a0363a1fd8578dbce76565313a743"
       "4"
       "3a656179f7313a76343a5554ad46313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e66b80c2d6d76ec07b171e"
       "a"
       "831ae0dc32c20a65353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64c3a97d5f0fb0a8c529a1c1533413d3f98ba8433d455a1ae94"
       "e"
       "640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e6466a0b40158a30acf8"
       "d"
       "bcd04cdaea1da07205dce92533c8d54e6591524d2374258eeae12222a375c3aa8a989b4"
       "d"
       "6cdfbaa4544e65fd7c66199d686ddefb83c23057bf5db5091c5b89d241c3514e6523094"
       "6"
       "2d4f78be71de2192396ce83eb502e825c9059a17754e657249fa6c7762ea25e96fdb559"
       "a"
       "dd4effccca5d97a6f22de5353a746f6b656e32303a982d9c2801010e3ce6bee9261e7e1"
       "e"
       "129982a995363a76616c7565736c363a0231424ddbbc363a76899c392327363a5e31ba8"
       "e"
       "624b363a5e3d8d268521363a4b4a6d357dda363ac9fc9d02636d363a567b909305b1363"
       "a"
       "7bcac9eecab3363a4e3cb6a7d804363a56635b22647c363a25a81900cd7c363a735779c"
       "c"
       "452e363a8de2a636a06e363a3ff53b26ca51363a4c686aba2c32363abe52294e9918363"
       "a"
       "1b22324e61a4363a5663650d2717363a3e9754a6e39f363ab19e985365a0363a4d7d79d"
       "6"
       "fde2363a0264402750df363a51a1cc6d2b8f363abf2169219edf363a5d2307e569c6363"
       "a"
       "3e3913a3c5cf363abef7c8cd2327363a4f731cad6205363a5db913543201363a4d4b58d"
       "e"
       "8412363a0250e18ca4d8363abd67b9945484363acbbe95455edb363a4fb210235058363"
       "a"
       "df180ab492ae6565313a74343a6176ed61313a76343a5554ad46313a79313a7265",
       "64313a7264323a696432303a4e619c4a782f841d74615737316a143b6e14587a323a697"
       "0"
       "343a51e8520d353a6e6f6465733230383a4e67960a585db1fef07be10936b1683a901cf"
       "f"
       "3654f82b133b3d4e66b80c2d6d76ec07b171ea831ae0dc32c20a654d8a99fafddb4e645"
       "4"
       "49f1f1bbe9ebb3a6db3c870c3e99245e52ad31ff63906a4e654e2cd9a20cef8eccf34a9"
       "0"
       "d009e1811627df67fcc8702bf74e649f171fa2e4e23b77f3a38751158619feacba6d7b9"
       "b"
       "1fc8d54e66fb0f1231710a49e10f535b58e7edc92086296167f34c23274e65565256018"
       "d"
       "2faa89acef502180e2693d3a0447e7b5fe4b394e66bba1b72abb596bcf5fccafae20042"
       "a"
       "53bfbd5f8c1f2d1ae1353a746f6b656e343ac80e1e1b363a76616c7565736c363a52dde"
       "d"
       "97e860363a5b86916b2779363a5d2307e569c6363abdd842cb2fd7363acbbe95455edb6"
       "5"
       "65313a74343a66639e36313a76343a4c54000f313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e66bba1b72abb596bcf5fc"
       "c"
       "afae20042a53bfbd353a6e6f6465733230383a4e6576aab410d63b8aaaea260d9d3f38a"
       "8"
       "ff0bcdad5e55f6c4914e65565256018d2faa89acef502180e2693d3a0447e7b5fe4b394"
       "e"
       "6423e9e355109ae9fa2a87c12c4b9bf6dbdb51c39a4dc4d75b4e652d5440b6536d7f980"
       "d"
       "617965bb5dbc3e89225f5e0419797d4e652309462d4f78be71de2192396ce83eb502e82"
       "5"
       "c9059a17754e642b9577d9ade16c785bfe50a8543aca66288c4f6ec8531ae94e6591524"
       "d"
       "2374258eeae12222a375c3aa8a989b4d6cdfbaa4544e650047deb3124dc843bb8ba61f0"
       "3"
       "5a7d0938065f495687d98e313a7069313030303065353a746f6b656e343a50a4c86f363"
       "a"
       "76616c7565736c363a0264402750df363a1b22324e61a4363a5559ad8ab237363a5a403"
       "6"
       "ef63a8363a5d2307e569c6363a5e31ba8e624b363a66fc0f568cd9363a6962b85e67703"
       "6"
       "3a70c64c87f743363ab19e985365a0363ab2d502706854363abb02f9f7b879363ac89b8"
       "a"
       "22c9716565313a74343a6663c17a313a76343a4c540101313a79313a7265",
       "64313a7264323a696432303a4e6359d8bb73d40b24475b59ea91bf3c5445cf73353a6e6"
       "f"
       "6465733230383a4e649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54e64f"
       "8"
       "39082811a76461bfd28268c4b73b445aa55916abd8d9954e6423e9e355109ae9fa2a87c"
       "1"
       "2c4b9bf6dbdb51c39a4dc4d75b4e65832b99e461d9320e265c1f72856ac25884ab5f441"
       "b"
       "dd1ae14e652309462d4f78be71de2192396ce83eb502e825c9059a17754e66fb0f12317"
       "1"
       "0a49e10f535b58e7edc92086296167f34c23274e66364026c24ef382f1fc570fb1c4ac5"
       "1"
       "5382b4b75d1e4d24434e674903d58543a8a61d2c4dcb87251a594041e445926537afc03"
       "5"
       "3a746f6b656e383ac509afe4c31dbc43363a76616c7565736c363ac877e3230001363ab"
       "3"
       "61f69a0001363a97483edf0001363ac3f2d570498b363a05c5cb8e0001363a4f150d3a0"
       "0"
       "01363a589cbb5b0001363abbbe15da0001363ab332cc570001363a5ebb328f0001363ab"
       "d"
       "db79620001363a2e74708b0001363a6d4a9e060001363a5d22ebb80001363a56fd658e0"
       "0"
       "01363a5631e3810001363acbbe95455edb363a505948f50001363acb4cdc620001363a6"
       "9"
       "4919860001363a7c08df9d41f3363ab157a77a0001363a1b22324e61a4363a58cb204b0"
       "0"
       "01363a697053c30001363a2e631faf0001363a5417c3580001363ac5fb81c80001363a4"
       "c"
       "686aba2c32363a050f31680001363a4fb534db0001363a55f7cc950001363a461e05b90"
       "0"
       "01363a5d2307e569c6363a0264402750df363abe64f99e0001363a5f020b8b0001363ac"
       "f"
       "cc495e0001363a5e436d870001363a74481d430001363a44214e570001363a56bfab2d0"
       "0"
       "01363ad05352c20001363aba0985a20001363a971406400001363ac50256dd0001363ac"
       "5"
       "fe4c060001363abe1232d10001363a540178ea0001363a5940318e00016565313a74343"
       "a"
       "666333d3313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e65832b99e461d9320e265"
       "c"
       "1f72856ac25884ab353a6e6f6465733230383a4e640c5bc5f1491016b0cd3fd2279a2d6"
       "0"
       "0e266754d5245dc8d54e642bb214ad57e9ad62f32122efcf4cac75135459b80e84e8a64"
       "e"
       "649f171fa2e4e23b77f3a38751158619feacba6d7b9b1fc8d54e64c3a97d5f0fb0a8c52"
       "9"
       "a1c1533413d3f98ba8433d455a1ae94e642b9577d9ade16c785bfe50a8543aca66288c4"
       "f"
       "6ec8531ae94e647cd6ae529049f1f1bbe9ebb3a6db3c870ce1c16fdef96f444e645449f"
       "1"
       "f1bbe9ebb3a6db3c870c3e99245e52ad31ff63906a4e6423e9e355109ae9fa2a87c12c4"
       "b"
       "9bf6dbdb51c39a4dc4d75b313a7069313030303065353a746f6b656e343ade807696363"
       "a"
       "76616c7565736c363a022890b41562363a0264402750df363a050e56f53314363a05168"
       "6"
       "beef00363a1b22324e61a4363a1fb0e372e41b363a1fcbac576c47363a257441f4d68b3"
       "6"
       "3a25a9974915e0363a25ab01e30def363a2732c9d8f2ab363a29a27cba1de2363a29e17"
       "f"
       "466f66363a2e74708bcc6b363a2f988542a153363a3defd17e95f1363a3ec978347e7d3"
       "6"
       "3a4c686aba2c32363a4d31be173aac363a4f52fdfcd8c3363a4f945a71578f363a51c62"
       "2"
       "489b06363a528c9fcc848d363a530ad58367f4363a540178ea3007363a5631e3812e483"
       "6"
       "3a5662059ef18c363a5679ab14d777363a56bfab2de950363a56ee3c17ad7c363a58635"
       "e"
       "466ad0363a586638673160363a59b033710568363a5b730437af56363a5cf0b1b634693"
       "6"
       "3a5d2307e569c6363a5d2867410400363a5e31ba8e624b363a5ebdc0f8e78f363a5ed11"
       "0"
       "097a48363a5fba3c760739363a5ff849ef8fa6363a699d8f8261af363a69e14d84a42a3"
       "6"
       "3a6e36ee9a7d2e363a7664e74365f9363a97140640b237363ab186d21bda65363ab19e9"
       "8"
       "5365a0363ab2c1f9595fb8363ab361f69a6ca7363ab946ac2674af363abb8654b9de573"
       "6"
       "3abbce27d429b7363abbe38c97cdb8363abc4619d466f1363abddabd061b37363abddb7"
       "9"
       "62ca89363ac3f2d570498b363ac55da29d16fc363acb4cdc62726b363acb4cdc62ceac3"
       "6"
       "3acfcc495e78d5363ad44c7603e96d363ad5953d14b7b6363ad94bdf28b1686565313a7"
       "4"
       "343a62794c87313a76343a4c540102313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e67f649f1f1bbe9ebb3a6d"
       "b"
       "3c870c3e99245e52353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64c3a97d5f0fb0a8c529a1c1533413d3f98ba8433d455a1ae94"
       "e"
       "642b9577d9ade16c785bfe50a8543aca66288c4f6ec8531ae94e6463d52f0f55c1ef812"
       "7"
       "12eef0d845c463e29dcb3b16a4dafd4e65832b99e461d9320e265c1f72856ac25884ab5"
       "f"
       "441bdd1ae14e6591524d2374258eeae12222a375c3aa8a989b4d6cdfbaa4544e6523094"
       "6"
       "2d4f78be71de2192396ce83eb502e825c9059a17754e657a6a4fd1521752c2188e85436"
       "1"
       "5085fd63eec0decd6ac8d5353a746f6b656e32303ac0024c89b2c3d65b2ed16cb9e1e90"
       "9"
       "7d667c51ca363a76616c7565736c363ab19e985365a06565313a74343a65611d0f313a7"
       "6"
       "343a5554ad46313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e674903d58543a8a61d2c4"
       "d"
       "cb87251a594041e4353a6e6f6465733230383a4e657a6a4fd1521752c2188e854361508"
       "5"
       "fd63eec0decd6ac8d54e64c3a97d5f0fb0a8c529a1c1533413d3f98ba8433d455a1ae94"
       "e"
       "65565256018d2faa89acef502180e2693d3a0447e7b5fe4b394e6494a636dfdd47b8dea"
       "1"
       "62b849ffe2dc89bffd6dfc53f048ae4e6423e9e355109ae9fa2a87c12c4b9bf6dbdb51c"
       "3"
       "9a4dc4d75b4e640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e649f171"
       "f"
       "a2e4e23b77f3a38751158619feacba6d7b9b1fc8d54e646f282e7fd6517ef5c10a194f7"
       "7"
       "5d9ef383af5d4c173668ec313a7069313030303065353a746f6b656e343a1a353f5f363"
       "a"
       "76616c7565736c363a0264402750df363a1b22324e61a4363a4c686aba2c32363a50648"
       "0"
       "c3f772363a5d2307e569c6363a5e31ba8e624b363a7664e74365f9363ab19e985365a03"
       "6"
       "3abc4619d466f1363ac3f2d570498b6565313a74343a62791707313a76343a4c5401023"
       "1"
       "3a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e63211e8cbe2eebda26f83"
       "b"
       "1e81da160dc208a4353a6e6f6465733230383a4e652258a3e994504f69cbc9d453e436a"
       "3"
       "e2811f2590368f64c54e6463d52f0f55c1ef812712eef0d845c463e29dcb3b16a4dafd4"
       "e"
       "65565256018d2faa89acef502180e2693d3a0447e7b5fe4b394e66fb0f1231710a49e10"
       "f"
       "535b58e7edc92086296167f34c23274e6403ef880050163a6599d6516b699a5e5e170fb"
       "d"
       "36af1e1ae14e67402166ead94933f4a563ac788f88d37d4d216d7a917656dd4e679a40c"
       "d"
       "4c1931912a94527bca70edc0182a625135b36f1ae14e67c3fc6a7bf4d520606d99ae755"
       "e"
       "b9040888598b2f1d475a9a353a746f6b656e343a652a985f363a76616c7565736c363a3"
       "d"
       "463b69cf83363a5d2307e569c6363a5e31ba8e624b363ab19e985365a0363ab52c782bc"
       "b"
       "c6363ac3f2d570498b6565313a74343a656146f5313a76343a4c540010313a79313a726"
       "5",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e66d03355f37878be49ff8"
       "9"
       "d661dad79954893a353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64e9b967d13c066d2609b25ea95bd9cf5b27ccaf2a57c46d174"
       "e"
       "640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8d54e647cd6ae529049f1f1b"
       "b"
       "e9ebb3a6db3c870ce1c16fdef96f444e6591524d2374258eeae12222a375c3aa8a989b4"
       "d"
       "6cdfbaa4544e65cc9d2ce22d471dfd5e0d305792eed602abc2b0e2ab96bf694e6523094"
       "6"
       "2d4f78be71de2192396ce83eb502e825c9059a17754e657a6a4fd1521752c2188e85436"
       "1"
       "5085fd63eec0decd6ac8d5353a746f6b656e32303a10f4017192d33d7ab6fe6dadb65a7"
       "a"
       "03579b7396363a76616c7565736c363a5d2307e569c6363a1b22324e61a46565313a743"
       "4"
       "3a656121e2313a76343a5554ab8d313a79313a7265",
       "64323a6970363a51e8520d2710313a7264323a696432303a4e66edf1f1bbe9ebb3a6db3"
       "c"
       "870c3e99245e0d90353a6e6f6465733230383a4e649f171fa2e4e23b77f3a3875115861"
       "9"
       "feacba6d7b9b1fc8d54e64d0d6ae529049f1f1bbe9ebb3a6db3c870ce1d571f946cdc94"
       "e"
       "6423e9e355109ae9fa2a87c12c4b9bf6dbdb51c39a4dc4d75b4e6463d52f0f55c1ef812"
       "7"
       "12eef0d845c463e29dcb3b16a4dafd4e65ae582944604273f8724346be144cc4fc99ffb"
       "c"
       "064b1e47214e65e57120db33aab99530aea347dbf43895a0b83bbbdccbbfeb4e6523094"
       "6"
       "2d4f78be71de2192396ce83eb502e825c9059a17754e6576aab410d63b8aaaea260d9d3"
       "f"
       "38a8ff0bcdad5e55f6c491353a746f6b656e32303aa2fc9b95c40b7581b7e8b8af5916e"
       "e"
       "b26e76c621363a76616c7565736c363a8a4bfeb14dac363abd643d364492363aaf8ef9b"
       "8"
       "a504363ab39f3a8d2c78363a4f23ef1339d2363a1b22324e61a46565313a74343a65619"
       "9"
       "fa313a76343a5554ad46313a79313a7265"});

  for (const char *c : hex) {
    sp::byte b[4096] = {0};
    std::size_t l = std::strlen(c);
    FromHex(b, c, l);
    sp::Buffer buffer(b);
    buffer.length = l;
    {
      {
        sp::Buffer copy(buffer);
        sp::bencode_print(copy);
      }
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
