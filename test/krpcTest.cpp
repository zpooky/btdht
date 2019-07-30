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

  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);

  {
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::request::ping(buff, t, id));
    sp::flip(buff);

    krpc::ParseContext ctx(dht, buff);
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

    krpc::ParseContext ctx(dht, buff);
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

  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);

  { //
    sp::Buffer buff{b};
    ASSERT_TRUE(krpc::request::find_node(buff, t, id, id));
    sp::flip(buff);

    krpc::ParseContext ctx(dht, buff);
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

    krpc::ParseContext ctx(dht, buff);
    test_response(ctx, [&id, &in](sp::Buffer &p) {
      dht::NodeId sender;
      if (!bencode::d::pair(p, "id", sender.id)) {
        return false;
      }
      assert_eq(sender.id, id.id);
      //
      sp::UinStaticArray<dht::IdContact, 256> outList;
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

TEST(krpcTest, test_get_peers) {
  sp::byte b[2048] = {0};
  dht::NodeId id;
  nodeId(id);

  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);

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
    krpc::ParseContext ctx(dht, buff);
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
      sp::UinStaticArray<dht::IdContact, 256> outNodes;

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
    // dht:: *peer[16] = {nullptr};
    sp::UinStaticArray<dht::Peer, 256> peer;

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

  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);

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

    krpc::ParseContext ctx(dht, buff);
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

    krpc::ParseContext ctx(dht, buff);
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
        bencode_print(copy);
      }
    }
  }
}

TEST(krpcTest, print_error_debug2) {
  // assertx(false);

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
  // const char *hex =
  //     "64323a6970363ad54182502710313a7264323a696432303adcfdf8073ea8"
  //     "c7fe156fb96c05455a43672fa420313a706931303030306565313a74343a"
  //     "6169b1fa313a76343a4c540100313a79313a7265";
  const char hex[] =
      "64323a696432303a629a5c0eaf70a2485c32789b965ce602753b11ce353a"
      "6e6f6465736c64323a6970693130373837393634373665343a706f727469"
      "3130343532656564323a69706932333530353532393665343a706f727469"
      "3534373238656564323a69706937303033323137323565343a706f727469"
      "3433323532656564323a6970693334383738303239363665343a706f7274"
      "693539363734656564323a6970693337333734343935353965343a706f72"
      "74693534373238656564323a6970693231333832383937353565343a706f"
      "7274693130303139656564323a697069363132393535373665343a706f72"
      "74693537383832656564323a6970693239393737333630343565343a706f"
      "72746931363634656564323a6970693231303431343635343165343a706f"
      "7274693336333033656564323a6970693333373438343738323965343a70"
      "6f7274693531313830656564323a69706931383833393735323565343a70"
      "6f7274693434333639656564323a6970693236323735373432333465343a"
      "706f7274693138313331656564323a697069313233383232363039336534"
      "3a706f727469323031656564323a6970693333323535393033343265343a"
      "706f7274693539363734656564323a697069323137363330323334316534"
      "3a706f7274693136353637656564323a6970693133333334373332303465"
      "343a706f7274693533363635656564323a69706932303837373737343034"
      "65343a706f7274693431353530656564323a697069313537363437373533"
      "3365343a706f7274693136363734656564323a6970693433313036343833"
      "3765343a706f7274693534393834656564323a6970693537303036313636"
      "3065343a706f72746938363930656564323a697069343233313137313432"
      "3265343a706f7274693235333133656564323a6970693239393239383131"
      "373665343a706f7274693539363734656564323a69706933383839303039"
      "34383865343a706f7274693139343133656564323a697069333631313139"
      "3830323665343a706f72746938353433656564323a697069313231363432"
      "3137343065343a706f7274693134373039656564323a6970693131353034"
      "323639353065343a706f7274693537363236656564323a69706932303939"
      "38313933333665343a706f7274693635303931656564323a697069323532"
      "3938333535393865343a706f7274693439373334656564323a6970693332"
      "373739323439303965343a706f7274693536393133656564323a69706932"
      "39383233353834353765343a706f7274693337323239656564323a697069"
      "3239343630373433373365343a706f7274693530313637656564323a6970"
      "693238343636363133313565343a706f7274693133373231656564323a69"
      "706935303631343338323665343a706f7274693537383832656564323a69"
      "70693339343230333439343565343a706f7274693237343230656564323a"
      "69706932333934353331383965343a706f7274693534373238656564323a"
      "6970693235393537343838323165343a706f727469353435383065656432"
      "3a69706931323236353238383565343a706f727469313631373465656432"
      "3a69706931323236353238383565343a706f727469333939383265656432"
      "3a69706939363734353730343865343a706f727469353736323665656432"
      "3a6970693339353134363239353865343a706f7274693631333137656564"
      "323a6970693337363933373531363665343a706f72746935343732386565"
      "64323a6970693133393535343636393965343a706f727469313030313965"
      "6564323a6970693137303634353234313365343a706f7274693332333930"
      "656564323a6970693136383432333030373665343a706f72746932303036"
      "656564323a69706938373832333632323265343a706f7274693438323536"
      "656564323a6970693134303836333339333065343a706f72746933393031"
      "37656564323a6970693131333631383238363865343a706f727469353634"
      "3232656564323a6970693130333533343239313065343a706f7274693534"
      "373238656564323a6970693235343535363437393865343a706f72746935"
      "39363734656564323a6970693131313338393330333765343a706f727469"
      "3236373038656564323a6970693337353638353939383565343a706f7274"
      "693135393533656564323a69706939333131313738363165343a706f7274"
      "693534373238656564323a6970693237343330343635313065343a706f72"
      "74693230393437656564323a6970693239333430393035383165343a706f"
      "72746937333035656564323a6970693136333934393637393565343a706f"
      "72746934313333656564323a6970693136393937363532353065343a706f"
      "7274693534373238656564323a6970693132373836323039373065343a70"
      "6f7274693231303735656564323a6970693131383130393933343665343a"
      "706f7274693430313336656564323a697069313230303539363532366534"
      "3a706f7274693131303934656564323a6970693335343231313531393365"
      "343a706f7274693338373035656564323a69706932353333393336393833"
      "65343a706f7274693531303831656564323a697069323131323131313032"
      "3665343a706f7274693534373238656564323a6970693137353238353136"
      "333265343a706f7274693234383931656564323a69706937343634313835"
      "303265343a706f7274693338373735656564323a69706932323931323333"
      "32333765343a706f7274693538373235656564323a697069343835323733"
      "39363565343a706f7274693530383134656564323a697069333230303034"
      "3338303065343a706f7274693138373332656564323a6970693230323731"
      "353933393265343a706f7274693534373238656564323a69706934313334"
      "33323331313965343a706f72746935313730656564323a69706934323835"
      "39343930343665343a706f7274693337333136656564323a697069333534"
      "3639303737333765343a706f7274693538353233656564323a6970693231"
      "373633303233343165343a706f7274693233393931656564323a69706933"
      "35343435373438383865343a706f7274693534373238656564323a697069"
      "3139323932353734383665343a706f7274693330363633656564323a6970"
      "6936323335373838383565343a706f7274693237333637656564323a6970"
      "693139323238383231333365343a706f72746934323532656564323a6970"
      "693132383931393233333765343a706f7274693332373635656564323a69"
      "70693431393535343333373965343a706f7274693337353438656564323a"
      "6970693132333832323630393365343a706f727469313430323565656432"
      "3a69706938383839363236333765343a706f72746931313432656564323a"
      "6970693331373330343236323265343a706f727469313835363765656432"
      "3a69706935393834343934383365343a706f727469323935383765656432"
      "3a6970693235343238353931343465343a706f7274693537363236656564"
      "323a6970693237373134393435313465343a706f72746934313435366565"
      "64323a6970693333363630303333373865343a706f727469313036323865"
      "6564323a6970693239353639303931383965343a706f7274693335343232"
      "656564323a6970693237313133343839333065343a706f72746933313239"
      "37656564323a6970693132343138333032363565343a706f727469353736"
      "3236656564323a6970693339393831383937383165343a706f7274693936"
      "3733656564323a6970693135323232353330363165343a706f7274693435"
      "37656564323a6970693232363331333037313265343a706f727469383338"
      "38656564323a6970693133363136383033323165343a706f727469323136"
      "3734656564323a6970693234343737343030383265343a706f7274693534"
      "373238656564323a69706933343238383738363565343a706f7274693434"
      "323133656564323a6970693334343739363637353765343a706f72746931"
      "35333035656564323a6970693239343930343533343365343a706f727469"
      "3331393735656564323a6970693431313630383533323765343a706f7274"
      "693436333036656564323a6970693134393334383538363165343a706f72"
      "74693237393436656564323a6970693235303333343039383865343a706f"
      "72746932343338656564323a6970693238363432313634313165343a706f"
      "7274693534373238656564323a6970693330393330313537313565343a70"
      "6f7274693534373238656564323a6970693331363738363734353465343a"
      "706f7274693534393834656564323a69706934343032363638333665343a"
      "706f7274693634373136656564323a697069333839303738343834396534"
      "3a706f7274693339313637656564323a6970693538363234303334336534"
      "3a706f7274693537363637656564323a6970693339333937313639343765"
      "343a706f7274693537363236656564323a69706931383131303334373332"
      "65343a706f7274693533383735656564323a697069323330333834353231"
      "3065343a706f7274693133343730656564323a6970693333363730393036"
      "303865343a706f7274693534373238656564323a69706931373239393436"
      "35333965343a706f72746939363730656564323a69706935363634343331"
      "363265343a706f7274693539373937656564323a69706931343630373132"
      "30323565343a706f7274693537373237656564323a697069313035303136"
      "3734323465343a706f72746931313735656564323a697069313738333334"
      "3534393665343a706f7274693236303930656564323a6970693234353136"
      "373338393365343a706f7274693238343039656564323a69706932333931"
      "30333534323965343a706f7274693533383939656564323a697069343139"
      "3831343736393965343a706f7274693538333536656564323a6970693730"
      "3237383736373065343a706f72746939393433656564323a697069333937"
      "3234313632313365343a706f7274693538363738656564323a6970693131"
      "3731313638343865343a706f72746932343532656564323a697069353638"
      "38363937343165343a706f7274693233353330656564323a697069333333"
      "3332333233313365343a706f7274693534373238656564323a6970693237"
      "343636373036363965343a706f7274693632313432656564323a69706931"
      "383433383934313365343a706f7274693530383235656564323a69706934"
      "30343232383230373965343a706f7274693130373939656564323a697069"
      "3234373438323739363965343a706f7274693338333436656564323a6970"
      "6936313630383039343265343a706f7274693438383435656564323a6970"
      "6939393432343030383965343a706f7274693537363236656564323a6970"
      "6939333938363438333765343a706f7274693130303139656564323a6970"
      "6938383634363133363765343a706f72746937373535656564323a697069"
      "36323231303137393765343a706f7274693130303139656564323a697069"
      "3332333036383930363565343a706f72746934333039656564323a697069"
      "3337383135323930393365343a706f72746931323132656564323a697069"
      "3239313537323834353365343a706f7274693133323839656564323a6970"
      "693339393838373632303265343a706f7274693530353130656564323a69"
      "70693231363938313435393465343a706f7274693335393333656564323a"
      "6970693136313533333937303865343a706f727469343632343165656432"
      "3a6970693431343332343433323565343a706f7274693237303231656564"
      "323a69706932393631323537333365343a706f7274693534373238656564"
      "323a69706933383832323630383565343a706f7274693537363236656564"
      "323a697069373339343436363765343a706f72746935353733656564323a"
      "6970693230333537303733323165343a706f727469323730373165656432"
      "3a6970693136303637343732393165343a706f7274693131333139656564"
      "323a69706931343138323830313665343a706f7274693534373238656564"
      "323a6970693135373135313935373965343a706f72746934333331366565"
      "64323a6970693136333632333930343965343a706f727469353733363265"
      "6564323a6970693239303936303034373465343a706f7274693534373734"
      "656564323a6970693239363135303435393565343a706f72746935383832"
      "31656564323a69706931393635373734353565343a706f72746936333535"
      "31656564323a6970693238393135343138333365343a706f727469333832"
      "3933656564323a6970693233383438313231383965343a706f7274693330"
      "303233656564323a6970693130353537313634343465343a706f72746931"
      "303433656564323a6970693234313932333635333065343a706f72746935"
      "35333733656564323a6970693232313932333635333765343a706f727469"
      "38383531656564323a6970693130343334353236313265343a706f727469"
      "3633333633656564323a6970693334343435343136353365343a706f7274"
      "693132383238656564323a69706936323930363234333765343a706f7274"
      "693234353430656564323a69706932303336363435343765343a706f7274"
      "693534373238656564323a69706932343139373334363065343a706f7274"
      "693534373238656564323a6970693339323937373336353165343a706f72"
      "74693534373238656564323a6970693232353136313636323165343a706f"
      "7274693537363236656564323a6970693138313637303538323965343a70"
      "6f7274693535323430656564323a6970693236373136383239353865343a"
      "706f7274693633363833656564323a69706932353133343933333365343a"
      "706f7274693431393637656564323a69706938333632323339303265343a"
      "706f7274693336373039656564323a697069323539353734383832316534"
      "3a706f7274693631323334656564323a6970693335313934313639323665"
      "343a706f7274693337333136656564323a69706931333131363738353535"
      "65343a706f7274693332353835656564323a697069323731313333313534"
      "3565343a706f72746938393034656564323a697069323939383533323639"
      "3265343a706f7274693534373238656564323a6970693237393033393038"
      "393365343a706f7274693631323035656564323a69706933383233393134"
      "363965343a706f7274693631323130656564323a69706933323736333338"
      "32383065343a706f7274693339343037656564323a697069323737323532"
      "3631353965343a706f7274693338323332656564323a6970693532303538"
      "3932343465343a706f7274693130303139656564323a6970693138383830"
      "303037303765343a706f7274693138333931656564323a69706932373339"
      "30383839373465343a706f72746931343732656564323a69706939363834"
      "373338363165343a706f7274693130343431656564323a69706933343036"
      "36393132323365343a706f72746932303330656564323a69706932323931"
      "30333437333365343a706f72746933303933656564323a69706934303039"
      "35393036313465343a706f72746935363435656564323a69706932383930"
      "36323939393765343a706f7274693534373238656564323a697069383632"
      "37393834313265343a706f7274693130303139656564323a697069313632"
      "3836343832363965343a706f7274693536383838656564323a6970693333"
      "343034353939323765343a706f7274693537363236656564323a69706932"
      "383538363337353765343a706f72746938343132656564323a6970693333"
      "333932323538303065343a706f7274693132333932656564323a69706939"
      "363134353530333665343a706f7274693231323638656564323a69706933"
      "33323735343332313865343a706f7274693334393732656564323a697069"
      "3335343234323431313465343a706f7274693130303139656564323a6970"
      "6932333337303934383865343a706f7274693539333838656564323a6970"
      "693337383931383335383365343a706f7274693630383939656564323a69"
      "706935303736373636323965343a706f7274693435363031656564323a69"
      "70693431373430343232383565343a706f72746938313337656564323a69"
      "706936383736323039343465343a706f72746938343032656564323a6970"
      "693137363733353338353765343a706f72746932383234656564323a6970"
      "693132333832323630393365343a706f72746939313631656564323a6970"
      "693338313336313032363465343a706f7274693537363236656564323a69"
      "706939313033393933313265343a706f7274693138333935656564323a69"
      "70693332373039303639363265343a706f7274693535323737656564323a"
      "6970693335343435303733393765343a706f727469313033353665656432"
      "3a697069373231373533363565343a706f7274693531313130656564323a"
      "6970693133303435333635303865343a706f727469353736323665656432"
      "3a69706939383235383238363665343a706f727469333632393465656432"
      "3a6970693137343730313630313965343a706f7274693130303139656564"
      "323a6970693237333632313035323665343a706f72746931313134316565"
      "64323a6970693234323935373233353465343a706f727469353736323665"
      "6564323a6970693430323835323438373765343a706f7274693233303139"
      "656564323a6970693430383335343039393765343a706f72746935373632"
      "36656564323a6970693137333231343938343865343a706f727469313131"
      "3331656564323a69706938363436363735303565343a706f727469353134"
      "3936656564323a69706931333538303530303665343a706f727469353531"
      "3333656564323a69706938353234303333373865343a706f727469313030"
      "3139656564323a69706932353639373839383165343a706f727469313235"
      "3036656564323a6970693235343030363431393365343a706f7274693537"
      "363236656564323a6970693239393039383238313265343a706f72746933"
      "34363132656564323a6970693237333634303634343865343a706f727469"
      "3235393330656564323a6970693430353732363631333265343a706f7274"
      "693233323930656564323a6970693139313831303331323565343a706f72"
      "74693130303139656564323a6970693239373635343738353465343a706f"
      "7274693131383038656564323a6970693139313134383933333865343a70"
      "6f7274693633383135656564323a6970693134343632353339313865343a"
      "706f7274693437343431656564323a697069323234363830383335376534"
      "3a706f7274693433313233656564323a6970693330363239333333353765"
      "343a706f7274693434313733656564323a69706932333333373432353439"
      "65343a706f7274693539363734656564323a697069333437363439343137"
      "3465343a706f7274693537363236656564323a6970693138343233343938"
      "373065343a706f7274693539363734656564323a69706931393639313937"
      "37353265343a706f72746932333630656564323a69706931393131333032"
      "39393665343a706f7274693238323133656564323a697069333431373437"
      "3938373565343a706f7274693238363331656564323a6970693238343738"
      "393636363365343a706f7274693439393830656564323a69706932373337"
      "32353732353365343a706f7274693534373238656564323a697069323034"
      "3135343831323565343a706f7274693635353335656564323a6970693731"
      "3531353733343065343a706f7274693337333136656564323a6970693331"
      "393038343733303865343a706f7274693430313433656564323a69706933"
      "30373539383035343965343a706f72746938313638656564323a69706933"
      "30323631323237313665343a706f7274693432333330656564323a697069"
      "3431373430343232383565343a706f7274693633313736656564323a6970"
      "693136353930313139323665343a706f7274693537363236656564323a69"
      "706931353032383432313965343a706f7274693338333837656564323a69"
      "70693231383439363931353565343a706f7274693537363236656564323a"
      "69706934363938393332303165343a706f7274693232303631656564323a"
      "6970693237343333303835393365343a706f72746937333734656564323a"
      "6970693335313930303535333065343a706f727469363236343965656432"
      "3a69706938333134373435323165343a706f727469353936373465656432"
      "3a6970693137313431313437313165343a706f7274693537363236656564"
      "323a69706938363735343531383365343a706f7274693534373238656564"
      "323a6970693338333436353934323865343a706f72746936313130656564"
      "323a6970693430353534303938343265343a706f72746931303031396565"
      "64323a69706935313132313134373765343a706f72746935343732386565"
      "64323a6970693135353936303035363965343a706f727469343232333865"
      "6564323a69706932393131343932343165343a706f727469343437343565"
      "6564323a69706932393131343932343165343a706f727469343231383565"
      "6564323a6970693133343639363337313765343a706f7274693631363639"
      "656564323a69706938353138303234353965343a706f7274693534373238"
      "656564323a6970693134363530323636353365343a706f72746934393131"
      "34656564323a6970693230323631323639363565343a706f727469333835"
      "3332656564323a69706932303132363931373365343a706f727469363230"
      "3239656564323a69706935353530383765343a706f727469353534393665"
      "6564323a6970693336303931383038353065343a706f7274693537353231"
      "656564323a6970693136393130373932333065343a706f72746936333434"
      "36656564323a6970693230393531383832373065343a706f727469353736"
      "3236656564323a6970693331303832363437383065343a706f7274693130"
      "303139656564323a6970693331383633343234373165343a706f72746935"
      "34373238656564323a6970693430383432393133393465343a706f727469"
      "3237363538656564323a69706932333031333235353965343a706f727469"
      "35383037656564323a6970693136323436313337353065343a706f727469"
      "3537363236656564323a69706937373430363830383465343a706f727469"
      "33383931656564323a6970693138333135333639373865343a706f727469"
      "31313238656564323a6970693231363330323032393365343a706f727469"
      "38333034656564323a6970693331333431333230343165343a706f727469"
      "3539363734656564323a6970693432363437353334353365343a706f7274"
      "6936313136656564323a6970693136383639313338383765343a706f7274"
      "693537363236656564323a6970693236333435333939323965343a706f72"
      "74693330393635656564323a6970693431333234383137323465343a706f"
      "7274693435343338656564323a6970693332313734383230353765343a70"
      "6f7274693133393530656564323a6970693231373633303233343165343a"
      "706f7274693133323339656564323a697069313933323934303930396534"
      "3a706f7274693139333330656564323a6970693133393736393435303965"
      "343a706f7274693535333130656564323a69706932393735383839393939"
      "65343a706f7274693435323830656564323a697069323038393637333432"
      "3965343a706f727469343936656564323a69706931363636303033353136"
      "65343a706f7274693237343336656564323a697069333535323731373630"
      "65343a706f7274693136383739656564323a697069333131333638343431"
      "3365343a706f7274693139383332656564323a6970693233393134383037"
      "373965343a706f7274693539363734656564323a69706931323937323031"
      "38303165343a706f7274693538333934656564323a697069323235373230"
      "3338303765343a706f7274693537363236656564323a6970693337383531"
      "363837333465343a706f7274693434343739656564323a69706932313734"
      "37303437393365343a706f7274693439353830656564323a697069313037"
      "3837393634373665343a706f7274693130343532656564323a6970693131"
      "363031383334383965343a706f7274693539363734656564323a69706932"
      "34313432323431363565343a706f72746938393432656564323a69706931"
      "34323135353237333365343a706f727469313030656564323a6970693138"
      "343838343634393665343a706f7274693437353133656564323a69706936"
      "393430333631363665343a706f7274693134323232656564323a69706932"
      "37303739373236383865343a706f7274693532383934656564323a697069"
      "3233343939343036353665343a706f7274693630313333656564323a6970"
      "693336393138353436333865343a706f7274693532343132656564323a69"
      "70693233343739313638383665343a706f7274693333303034656564323a"
      "6970693236323832343430333065343a706f72746938313531656564323a"
      "6970693137393133303932333065343a706f727469353936373465656432"
      "3a6970693134363430393732303865343a706f7274693537363236656564"
      "323a6970693135363030373235323865343a706f72746934383334306565"
      "64323a6970693430393233313534333865343a706f727469353736323665"
      "6564323a6970693334333235313138333665343a706f7274693635313065"
      "6564323a69706934333330383231363265343a706f727469353736323665"
      "6564323a69706936333736383836383565343a706f727469313139383165"
      "6564323a6970693332313634303636303465343a706f7274693834323465"
      "6564323a69706939343930363331303265343a706f727469333535303565"
      "6564323a6970693131353538343433373665343a706f7274693538363138"
      "656564323a69706931363935393930343465343a706f7274693337333136"
      "656564323a6970693336343135333435373665343a706f72746933363431"
      "30656564323a6970693131313430363834313465343a706f727469353736"
      "3236656564323a69706935383330353932393965343a706f727469343636"
      "3330656564323a6970693239373438363031313865343a706f7274693433"
      "333338656564323a6970693431393130393636323765343a706f72746935"
      "37363236656564323a6970693133383837353131373165343a706f727469"
      "3537363236656564323a6970693431333936333831303865343a706f7274"
      "693232363434656564323a69706936303338353539363765343a706f7274"
      "693230363735656564323a69706938383138393339373165343a706f7274"
      "693536383135656564323a6970693231393732363736333465343a706f72"
      "74693631383531656564323a69706932313633343838343865343a706f72"
      "74693536373036656564323a6970693236323032383638313365343a706f"
      "7274693531333735656564323a6970693239303032373535333265343a70"
      "6f7274693538323632656564323a6970693337363336313835383465343a"
      "706f7274693236363634656564323a697069323239303936343431326534"
      "3a706f7274693136343835656564323a6970693339303436383038393465"
      "343a706f7274693235323234656564323a69706931303535353636363737"
      "65343a706f7274693537363236656564323a697069333832363039353033"
      "3365343a706f7274693132373339656564323a6970693434303438383339"
      "3965343a706f7274693537363236656564323a6970693137383938373733"
      "343365343a706f7274693333303336656564323a69706934303530333230"
      "31373465343a706f7274693537363236656564323a697069363434313737"
      "35303365343a706f7274693433393337656564323a697069313231343035"
      "3434383765343a706f7274693537363236656564323a6970693138323231"
      "333537363265343a706f7274693230383639656564323a69706932333830"
      "31333738323365343a706f7274693537363236656564323a697069353232"
      "36303436393565343a706f7274693534373238656564323a697069333132"
      "3339353832303765343a706f7274693230353631656564323a6970693236"
      "373632343931373965343a706f7274693635313735656564323a69706932"
      "32313330373334383265343a706f7274693434333231656564323a697069"
      "3131383534323530373265343a706f7274693534373238656564323a6970"
      "693238353531323034333565343a706f7274693436333332656564323a69"
      "706938343834343537383565343a706f7274693534373238656564323a69"
      "70693237373632303230373765343a706f7274693337333136656564323a"
      "6970693230303238363537353765343a706f727469333434383965656432"
      "3a69706935373631303834363365343a706f727469353936373465656432"
      "3a69706938383239343132353365343a706f72746935343135656564323a"
      "6970693133343934393531313965343a706f72746934656564323a697069"
      "3234313935343737323865343a706f7274693534373238656564323a6970"
      "693232383539313737383165343a706f7274693630333836656564323a69"
      "706933323739353931333565343a706f7274693535373432656564323a69"
      "70693236323936393932363065343a706f7274693534373238656564323a"
      "6970693230383332343838343365343a706f727469343433323165656432"
      "3a6970693233363536363937313765343a706f7274693332343432656564"
      "323a6970693431303834393235383165343a706f72746939313533656564"
      "323a6970693133333534343939323965343a706f72746934303831326565"
      "64323a69706935363731393532323965343a706f72746932323636356565"
      "64323a6970693338363133353135313165343a706f727469313932373665"
      "6564323a69706937313536323735393565343a706f727469353437323865"
      "6564323a69706933323437353131303965343a706f727469343736303465"
      "6564323a6970693333353134353732303965343a706f7274693235373039"
      "656564323a6970693431373433333733363365343a706f72746935343732"
      "38656564323a6970693234393834363736373765343a706f727469353936"
      "3734656564323a6970693338373035343436303165343a706f7274693135"
      "343337656564323a6970693335323932353936383765343a706f72746935"
      "34393834656564323a6970693237303030373934303365343a706f727469"
      "3531373432656564323a69706932353834323937393165343a706f727469"
      "3632383434656564323a6970693137343333353036303665343a706f7274"
      "693534373238656564323a6970693336393634303936333765343a706f72"
      "74693532323933656564323a6970693134303130393334353965343a706f"
      "7274693137353037656564323a6970693232303530393839333165343a70"
      "6f72746937373338656564323a6970693134393839303635383165343a70"
      "6f7274693239353933656564323a697069353739353532363565343a706f"
      "7274693539363734656564323a6970693136363734333230333065343a70"
      "6f7274693537363236656564323a6970693234323136393831343165343a"
      "706f7274693537363236656564323a697069343136343534343135316534"
      "3a706f7274693539363734656564323a6970693534343531303237376534"
      "3a706f7274693439353339656564323a6970693133363935323638373265"
      "343a706f7274693635313132656564323a69706936363736333838373765"
      "343a706f7274693237333734656564323a69706932393235373937343536"
      "65343a706f7274693432323633656564323a697069393832333936373131"
      "65343a706f7274693232373239656564323a697069323335353339373239"
      "3565343a706f7274693537363236656564323a6970693432373933373333"
      "383165343a706f7274693537383832656564323a69706935313733313531"
      "363265343a706f7274693430373436656564323a69706931333138333132"
      "37383765343a706f72746935313738656564323a69706931363932323933"
      "37323765343a706f7274693530393633656564323a697069333638323139"
      "3837343765343a706f7274693533353834656564323a6970693139343337"
      "323736393365343a706f7274693337333136656564323a69706931353632"
      "36383238383565343a706f7274693430333733656564323a697069313636"
      "3836353336343965343a706f7274693538313035656564323a6970693931"
      "3236373234333465343a706f7274693534373238656564323a6970693430"
      "333533353735323365343a706f7274693539363734656564323a69706933"
      "32393236393538393765343a706f7274693437363934656564323a697069"
      "31333136333638393965343a706f7274693539363734656564323a697069"
      "3131393132383337393765343a706f7274693230383236656564323a6970"
      "693334333733393239383765343a706f7274693239303436656564323a69"
      "70693231333034323339383765343a706f7274693337333136656564323a"
      "6970693138373032323239353765343a706f727469343437333165656432"
      "3a6970693431343933353433333565343a706f7274693630333635656564"
      "323a6970693232353033373236373565343a706f72746935343732386565"
      "64323a6970693130363337303331333265343a706f727469313235323165"
      "6564323a6970693132343536363839383165343a706f7274693337333136"
      "656564323a6970693335373233313434313765343a706f72746932393336"
      "32656564323a6970693130303634303830343365343a706f727469323833"
      "3838656564323a69706934353238393730383465343a706f727469353437"
      "3238656564323a6970693332383835313339303165343a706f7274693233"
      "373936656564323a6970693134303238323439393765343a706f72746935"
      "37363935656564323a6970693135313934343235313765343a706f727469"
      "3538323739656564323a6970693335363831323432343165343a706f7274"
      "693238373937656564323a6970693233353639323630373365343a706f72"
      "74693633393538656564323a6970693138363630353332323965343a706f"
      "7274693230353830656564323a6970693139393430383236353365343a70"
      "6f7274693532393138656564323a6970693337373933373033303965343a"
      "706f7274693538333934656564323a697069313837343435343035376534"
      "3a706f7274693539353530656564323a6970693133383331333430373565"
      "343a706f7274693339383531656564323a69706933303232383139333635"
      "65343a706f7274693233333833656564323a697069313235343037383033"
      "3365343a706f7274693537363236656564323a6970693138373532363932"
      "333365343a706f727469343230656564323a697069333536333336323635"
      "3165343a706f7274693237383431656564323a6970693231333633393032"
      "343965343a706f7274693231323034656564323a69706938363538363739"
      "363565343a706f72746934313335656564323a6970693339363734373335"
      "383565343a706f7274693238313233656564323a69706931373138383739"
      "35363965343a706f727469313930656564323a6970693238323633323935"
      "313865343a706f7274693534373238656564323a69706933333235353139"
      "35333765343a706f7274693533343535656564323a697069323734353734"
      "3234323765343a706f72746938363232656564323a697069323631373138"
      "3038353965343a706f72746938393131656564323a697069323331353734"
      "37343165343a706f7274693337333136656564323a697069313534383432"
      "3530343265343a706f72746939343630656564323a697069313533353937"
      "3535303565343a706f7274693537363236656564323a6970693331343832"
      "383433353765343a706f7274693337333136656564323a69706931353532"
      "32383535353065343a706f7274693135313537656564323a697069353638"
      "303632333165343a706f7274693432303630656564323a69706931353533"
      "33393337303165343a706f7274693537363236656564323a697069343037"
      "3834353538373065343a706f72746937363138656564323a697069373030"
      "33323137323565343a706f7274693433323532656564323a697069323534"
      "3838373136383565343a706f7274693433353836656564323a6970693339"
      "393339373138303165343a706f7274693333393139656564323a69706933"
      "36353831363037333565343a706f7274693134303735656564323a697069"
      "3230383937303432313565343a706f7274693537383832656564323a6970"
      "693132383734393033373865343a706f7274693236303732656564323a69"
      "70693333333339323034373765343a706f72746935353130656564323a69"
      "70693336383033313336343965343a706f7274693337333136656564323a"
      "6970693239343832383133383165343a706f727469323038323265656432"
      "3a6970693130373635313630333965343a706f7274693537363236656564"
      "323a6970693239343832393337323565343a706f72746934313439656564"
      "323a6970693134383538393939353165343a706f72746935393637346565"
      "64323a6970693430303635393536323165343a706f727469333139333465"
      "6564323a69706935373936333235373065343a706f727469323733323965"
      "6564323a6970693338303139313434343365343a706f7274693630323034"
      "656564323a69706939363532393036363765343a706f7274693430353238"
      "656564323a6970693431333133393630313965343a706f72746933373331"
      "36656564323a6970693237313630363338333765343a706f727469343337"
      "3538656564323a6970693339343234303935373765343a706f7274693333"
      "333338656564323a6970693133343235363034333565343a706f72746935"
      "39383930656564323a6970693339343338303839393165343a706f727469"
      "37313936656564323a69706935343830393131373565343a706f72746935"
      "343033656564323a6970693239393333323837343965343a706f72746931"
      "34393938656564323a6970693130323037383730353765343a706f727469"
      "32333430656564323a69706937343139303136353765343a706f72746931"
      "38333631656564323a6970693432313531363137383365343a706f727469"
      "35343238656564323a6970693331393938313231373565343a706f727469"
      "3234343531656564323a6970693330363737303633303165343a706f7274"
      "6934383335656564323a6970693137303939383639313165343a706f7274"
      "693232333839656564323a6970693430313631393131353565343a706f72"
      "746934353138656564323a69706938303536323832353965343a706f7274"
      "693537363236656564323a69706938303536323832353965343a706f7274"
      "693538363530656564323a69706938303536323832353965343a706f7274"
      "693539313632656564323a6970693237313832343033333765343a706f72"
      "74693437303435656564323a6970693238363239353031373565343a706f"
      "7274693136313235656564323a6970693130343930383035333265343a70"
      "6f7274693132373539656564323a6970693330353933323136373765343a"
      "706f7274693130313031656564323a69706938313832313131353565343a"
      "706f7274693335343530656564323a697069313530323134313236376534"
      "3a706f7274693132656564323a6970693336323332313533313565343a70"
      "6f7274693537363236656564323a6970693234353639363535353565343a"
      "706f7274693337333136656564323a697069313334333838313035356534"
      "3a706f7274693537303237656564323a6970693431303833333639356534"
      "3a706f7274693336303533656564323a6970693235323039303538313565"
      "343a706f7274693531343137656564323a69706931303931303036303631"
      "65343a706f7274693233343336656564323a697069323332333436393530"
      "3165343a706f7274693630353234656564323a6970693135303231343132"
      "363765343a706f727469333034656564323a697069343134393935323935"
      "65343a706f7274693436353737656564323a697069333631303538333233"
      "3565343a706f7274693539363734656564323a6970693430363538363232"
      "3365343a706f7274693336393030656564323a6970693234363533363233"
      "363365343a706f7274693537393833656564323a69706933343130363233"
      "35393765343a706f7274693331303432656564323a697069353139333831"
      "39353565343a706f7274693435323939656564323a697069373133363835"
      "333365343a706f7274693337373032656564323a69706933363437353632"
      "323965343a706f7274693537363236656564323a69706932363635353935"
      "32353365343a706f7274693531373531656564323a697069313239333230"
      "3039343765343a706f7274693537363236656564323a6970693235383537"
      "3539353565343a706f7274693534393834656564323a6970693333333934"
      "343636323165343a706f7274693537363236656564323a69706934323435"
      "37303435343365343a706f7274693539363734656564323a697069333638"
      "3933303432333765343a706f72746936363239656564323a697069323539"
      "3332393434373765343a706f7274693534313836656564323a6970693337"
      "343639313636393365343a706f7274693539363734656564323a69706932"
      "303334363633333565343a706f727469353335656564323a697069313536"
      "3434383735323765343a706f7274693335363633656564323a6970693336"
      "383039313935393365343a706f72746938323331656564323a6970693238"
      "363634343436363665343a706f7274693537363236656564323a69706932"
      "35383439323634323565343a706f7274693435383639656564323a697069"
      "3133303537363736313165343a706f7274693335303933656564323a6970"
      "693133303537363736313165343a706f7274693335303933656564323a69"
      "70693230303535333635343365343a706f7274693534373238656564323a"
      "6970693232363337393438303765343a706f727469313030313965656432"
      "3a69706933343730343236353365343a706f727469313030313965656432"
      "3a6970693133363538323431383165343a706f7274693432313334656564"
      "323a6970693134333230363238393765343a706f72746931393930396565"
      "64323a6970693432303337353333313265343a706f727469323635353465"
      "6564323a6970693133363537373530333765343a706f7274693336313835"
      "656564323a6970693133363136313333363165343a706f72746932383934"
      "36656564323a6970693231393535353337323365343a706f727469393438"
      "33656564323a6970693332383236343232373165343a706f727469353738"
      "3832656564323a6970693332383236343232373165343a706f7274693465"
      "6564323a6970693239323933373339303865343a706f7274693138363365"
      "6564323a69706932373033313831363965343a706f727469353736323665"
      "6564323a69706932373033313831363965343a706f727469333930346565"
      "64323a6970693133373431363334363165343a706f727469353037383065"
      "6564323a69706934303031383235383265343a706f727469363034343265"
      "6564323a6970693137363831373734353365343a706f7274693631333234"
      "656564323a69706931333839313934393465343a706f7274693537363236"
      "656564323a6970693333343733303437353865343a706f72746936303434"
      "32656564323a6970693330343230353731353865343a706f727469353831"
      "3338656564323a6970693332323732323236363165343a706f7274693537"
      "363236656564323a6970693236323438373436363865343a706f72746935"
      "37363236656564323a6970693233333735383737363665343a706f727469"
      "3537363236656564323a6970693233323630333437373965343a706f7274"
      "6933393730656564323a6970693333343733303437353865343a706f7274"
      "693630343432656564323a69706939333030303936363265343a706f7274"
      "693538333934656564323a6970693231313630343839343365343a706f72"
      "74693236313531656564323a69706934303031383235383265343a706f72"
      "7469363034343265656565";

  sp::byte b[40960] = {0};
  std::size_t l = std::strlen(hex);
  FromHex(b, hex, l);
  sp::Buffer buffer(b);
  buffer.length = l;
  {
    {
      sp::Buffer copy(buffer);
      bencode_print(copy);
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
      bencode_print(copy);
    }
  }
}

TEST(krpcTest, print_find_node_debug) {
  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);
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
        bencode_print(copy);
      }

      sp::Buffer copy(buffer);
      krpc::ParseContext ctx(dht, copy);
      test_response(ctx, [](auto &p) {
        bool b_id = false;
        bool b_n = false;
        bool b_p = false;
        bool b_ip = false;
        bool b_t = false;

        dht::NodeId id;
        dht::Token token;

        sp::UinStaticArray<dht::IdContact, 256> nodes;

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
          clear(nodes);
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
  fd udp{-1};
  Contact listen;
  prng::xorshift32 r(1);
  dht::DHT dht(udp, listen, r);
  // const char hex[] =
  // "64313a7264323a696432303a17323a78dac46ada7f7b6d886fb28da"
  //                    "0cd4ae253323a6970"
  //                    "343ad5418250353a6e6f6465733230383a3ed1e36fd2a5a2fee56b9"
  //                    "9825c41a6ebb4c0d1"
  //                    "da464f699cf82b3ab799e9ebb3a6db3c870c3e99245e0d1c06b7f12"
  //                    "5bd31a8a0d933eb26"
  //                    "f5bb58a36d01235d763e593c46189645d5d9e260bb1ae92917114b0"
  //                    "768f8e04cce67ee57"
  //                    "4f418c32dd7be3b55fe1f3742c281d88d6ae529049f1f1bbe9ebb3a"
  //                    "6db3c870ce1ae076c"
  //                    "c33ae6212f63095b51765f749c287a1076cc560fb608a8576e4dc21"
  //                    "ae1212f71e9ebb3a6"
  //                    "db3c870c3e99245e0d1c06b7f15bc43709317d21185aca77c818b20"
  //                    "0653841da665f6b1e"
  //                    "751d92ddf1c3bbc8d565313a74343a657545b3313a76343a5554a5b"
  //                    "1313a79313a7265";

  const char hex[] =
      "64323a697032303a629a5c0eaf70a2485c32789b965ce602753b11ce353a"
      "6e6f6465736c64323a6970adb39624343a706f72748fa46564323a697058"
      "6c04ab343a706f7274c4916564323a6970601e65ad343a706f7274560565"
      "64323a6970d04c5d13343a706f72741ae16564323a6970a3ac4b5f343a70"
      "6f727421236564323a6970c0a8020e343a706f7274c8d56564323a6970bc"
      "3cec72343a706f727443ab6564323a6970ca8e84ab343a706f72741ae965"
      "64323a6970b2c06531343a706f72741ae96564323a69705ed166b2343a70"
      "6f72741ae96564323a69707646b6cf343a706f72747a936564323a697054"
      "c1b75d343a706f72741ae96564323a6970bae7718a343a706f7274305d65"
      "64323a6970c0a8010c343a706f72740d6b6564323a697034d12772343a70"
      "6f72741fb46564323a6970601e65ad343a706f727456056564323a697099"
      "c9d30f343a706f7274d9036564323a6970d5884f07343a706f72742e3965"
      "64323a69705762a258343a706f72741ae16564323a6970a3acdc79343a70"
      "6f72741ae16564323a69709f416468343a706f7274c3576564323a69705b"
      "798916343a706f7274c8d56564323a69702e2f85be343a706f7274eb1a65"
      "64323a6970601e65ad343a706f727456056564323a69704c44ad16343a70"
      "6f7274c8d56564323a6970b4bf157f343a706f72741ae16564323a69706d"
      "ba6a7d343a706f7274cf8d6564323a6970551728c9343a706f7274ecc765"
      "64323a6970d5884f1b343a706f72742fc76564323a6970d5b73a0b343a70"
      "6f727451ad6564323a697058c42972343a706f727480036564323a697025"
      "bb729f343a706f72747a2d6564323a69703ed2fc51343a706f7274d76f65"
      "64323a69706eaf4671343a706f7274d0e06564323a697002049a9a343a70"
      "6f7274c44f6564323a697047d5f834343a706f7274d36e6564323a6970c8"
      "9430ea343a706f72744f6a6564323a697025bb70de343a706f7274c8d565"
      "64323a69705b790132343a706f7274c8d56564323a697058cfc39d343a70"
      "6f72741ae96564323a697005274d44343a706f727486fc6564323a6970ad"
      "aac922343a706f7274aa5b6564323a69709f9322e0343a706f72741ae165"
      "64323a69707d812bb6343a706f7274ed8e6564323a6970616ae5ea343a70"
      "6f7274fa876564323a69707e440f7b343a706f727499346564323a69709e"
      "8c886b343a706f72741ae16564323a6970488d8435343a706f72741aea65"
      "64323a69704b539f01343a706f7274c4916564323a6970b014a449343a70"
      "6f7274f8c26564323a697053c42639343a706f7274e0456564323a69705c"
      "73fa21343a706f7274f2216564323a6970c39aa4b0343a706f7274e3a465"
      "64323a69704a823ed7343a706f72745f216564323a69706c1f8148343a70"
      "6f727475396564323a6970461f9244343a706f72741ae16564323a697048"
      "b3287d343a706f727443fe6564323a69704e3aca96343a706f727446c265"
      "64323a6970c39ab564343a706f7274c8d56564323a6970461ef33e343a70"
      "6f7274db796564323a6970d5884f1b343a706f727417566564323a6970c3"
      "9aa73f343a706f7274d6d86564323a697005bdbb5a343a706f72747a7465"
      "64323a6970490b554c343a706f727499596564323a69701b54ae24343a70"
      "6f7274c4ef6564323a697068eea91f343a706f72748bcc6564323a697046"
      "b01a2f343a706f727486cc6564323a697018e1f6f3343a706f72741ae165"
      "64323a697005bdbb5a343a706f72741b016564323a69703ed2d902343a70"
      "6f7274c8d56564323a69706cf996c0343a706f72741ae16564323a6970b2"
      "21ef3c343a706f7274c8d56564323a697095381d0e343a706f727475f765"
      "64323a697050da0b25343a706f72741ae16564323a69700587a6af343a70"
      "6f7274ca356564323a69708ba24bc5343a706f72741aa26564323a6970b2"
      "cf1207343a706f7274c8d56564323a697005bdbc17343a706f7274c85a65"
      "64323a6970add4cd04343a706f72741aca6564323a6970bb7271ef343a70"
      "6f7274c4916564323a697059d4680a343a706f72745b4a6564323a6970b4"
      "7212d1343a706f72742a616564323a69702e8a86eb343a706f727485ef65"
      "64323a697025bb7cec343a706f72741ae16564323a69702eea7a26343a70"
      "6f72741ae16564323a69700298b3af343a706f72741ec96564323a6970d5"
      "884fee343a706f727482ca6564323a69703ed26d5d343a706f7274d76f65"
      "64323a69706d0a9d24343a706f7274c8d56564323a69705d163d8e343a70"
      "6f7274d0406564323a69703ed2b6a0343a706f7274d7286564323a697025"
      "bb7463343a706f7274c8d56564323a69704b9bcc49343a706f7274c8d565"
      "64323a6970054f4d5f343a706f7274a4736564323a69703bbe5ab7343a70"
      "6f72741ae96564323a697043546792343a706f72741ae16564323a697005"
      "8b11c3343a706f7274def56564323a6970259c4ff7343a706f7274c8d565"
      "64323a6970c39ab302343a706f7274b0646564323a6970c15dd925343a70"
      "6f727464b96564323a697005bdb939343a706f72741b266564323a6970ad"
      "d4cd49343a706f7274c8f96564323a69703ed2470d343a706f7274d73365"
      "64323a6970b220dcbc343a706f7274c8d56564323a697043b5d47e343a70"
      "6f7274c4916564323a69700e8e764b343a706f7274598d6564323a697055"
      "4bac8e343a706f72745bcb6564323a69706db1ec1c343a706f72747ec665"
      "64323a697062e4f877343a706f727423276564323a6970d5884f07343a70"
      "6f72741b386564323a697005bdbb5a343a706f72747a586564323a69703e"
      "14b63d343a706f7274c8d56564323a697005bdb939343a706f72740f7465"
      "64323a69707de6052c343a706f72745cc76564323a69704f7a56f5343a70"
      "6f7274e2b46564323a697025c90459343a706f72742a6d6564323a6970bc"
      "f33595343a706f727486096564323a69705b79b8aa343a706f7274c8d565"
      "64323a6970b28ce591343a706f7274c8d56564323a6970b90d7014343a70"
      "6f7274b5ac6564323a697025cc83cd343a706f7274c93b6564323a69705f"
      "dcc6af343a706f7274e77c6564323a697054f03d1a343a706f7274ccfc65"
      "64323a697005bdb781343a706f7274b75d6564323a69705ffe7bb5343a70"
      "6f7274f1aa6564323a69707cab952d343a706f727488416564323a697005"
      "bdb939343a706f7274c8e86564323a697005bdbb5a343a706f7274c92565"
      "64323a69705205950b343a706f72741ae16564323a69705e9e448b343a70"
      "6f7274e7286564323a69701b5bc57a343a706f7274c8d56564323a697005"
      "0f2b25343a706f7274e76a6564323a697055da9c72343a706f72749c1065"
      "64323a69709183d74c343a706f7274fd7f6564323a697053e912fa343a70"
      "6f7274ac926564323a6970b99df564343a706f7274d9056564323a6970b0"
      "b7fc5c343a706f7274c8d56564323a6970d5fbb79a343a706f7274332165"
      "64323a6970253be029343a706f7274c0076564323a6970330f0c1d343a70"
      "6f7274ddb76564323a6970ddd866df343a706f727408116564323a6970b9"
      "9c98ed343a706f7274e3d26564323a69705d7e443b343a706f7274c8d565"
      "64323a69704f4380c0343a706f72741ae16564323a697052e0495b343a70"
      "6f7274c8d56564323a6970522eb877343a706f727421ad6564323a6970b9"
      "16adc6343a706f7274c8d56564323a69704ddeb6a3343a706f7274bef265"
      "64323a697059b21057343a706f72747fe16564323a69708048983e343a70"
      "6f727497046564323a69702ea6b824343a706f7274cdbe6564323a697059"
      "e6423b343a706f72741ae16564323a697049cce104343a706f7274d7e865"
      "64323a6970bf274f86343a706f727493ec6564323a6970296790c0343a70"
      "6f7274d5106564323a697073a63afa343a706f7274f4e36564323a697056"
      "b0e329343a706f7274d7266564323a6970d542c6ec343a706f727436e565"
      "64323a6970534fd3ea343a706f72741ae16564323a69705fde1d34343a70"
      "6f72741a576564323a697049e2f0a1343a706f72741ae96564323a697002"
      "5ec75e343a706f727442ff6564323a697005bdbc17343a706f72741b0565"
      "64323a697005bdb781343a706f72746d266564323a69704ff78a3c343a70"
      "6f7274f6f36564323a697018040554343a706f72741ae96564323a697005"
      "87a604343a706f7274c8d56564323a6970258ffd0a343a706f727489c665"
      "64323a69705f54f0f0343a706f72742f2a6564323a6970c1e08293343a70"
      "6f7274ca956564323a69703ed242e1343a706f72741ae16564323a697025"
      "932192343a706f7274f96e6564323a6970254e848e343a706f72748bd265"
      "64323a69706dbbeac4343a706f727465e56564323a69706d43e821343a70"
      "6f7274ea5b6564323a69703278495f343a706f7274675f6564323a697005"
      "bdbc17343a706f7274b7366564323a697051aae8e7343a706f7274ff9865"
      "64323a69705e0ac6d1343a706f7274c4916564323a69705b4e6804343a70"
      "6f7274c5156564323a69704ec4cb88343a706f72741ae96564323a6970da"
      "fa6cad343a706f7274f6d56564323a6970bc204860343a706f7274a1b465"
      "64323a697053f984b0343a706f7274c5e56564323a69706d8035b1343a70"
      "6f72742d376564323a6970640ee8ba343a706f727459826564323a69705b"
      "7cab5d343a706f727434a96564323a6970c9068761343a706f727412e065"
      "64323a6970af88b70b343a706f72743ff86564323a6970496d59ac343a70"
      "6f727495956564323a69709d58258e343a706f727447756564323a6970b9"
      "2dc3bc343a706f72746d796564323a6970050f27cc343a706f72741ae165"
      "64323a69705cf4ec3e343a706f727413046564323a6970b29e3290343a70"
      "6f72744dd86564323a69708f9f2c6c343a706f7274d5d66564323a69703e"
      "3992b6343a706f7274ca906564323a6970029b26b5343a706f7274427665"
      "64323a6970d9429d5a343a706f7274650f6564323a69709985078a343a70"
      "6f7274278d6564323a6970c340d0e6343a706f727406fa6564323a6970d4"
      "386c0e343a706f7274c8d56564323a697053963bea343a706f7274c8d565"
      "64323a697055b62c16343a706f72741ae16564323a697025bb7e25343a70"
      "6f7274dc5f6564323a6970bc8aad39343a706f7274e3fa6564323a697091"
      "ff0518343a706f7274c8d66564323a6970a39e7ed4343a706f72741ae965"
      "64323a69703ed2aa23343a706f7274d76f6564323a6970501fe097343a70"
      "6f72741ae16564323a69705e276bb0343a706f72741ae96564323a69705b"
      "45d5e1343a706f7274c2a16564323a6970054f48a6343a706f7274c8d565"
      "64323a69706d834bac343a706f7274c8d56564323a69705ba02e4e343a70"
      "6f7274497f6564323a697054fab9b2343a706f7274c8d56564323a6970b0"
      "7c8e4a343a706f72741ae96564323a69706dfc51a6343a706f727415ef65"
      "64323a6970d53cef73343a706f7274d01b6564323a697005bdb781343a70"
      "6f7274c8446564323a6970d5884fcd343a706f7274c2a76564323a6970ad"
      "d4cd49343a706f7274c9236564323a69704d331361343a706f727438de65"
      "64323a69704def0911343a706f7274dc206564323a6970bca34e39343a70"
      "6f727414536564323a6970b24756c6343a706f72749c886564323a69704f"
      "6441a5343a706f727458956564323a6970975f1bc7343a706f72741ae165"
      "64323a6970add4ca16343a706f72741aef6564323a697068f448c3343a70"
      "6f7274ef996564323a6970c88a08c7343a706f727468306564323a697032"
      "1a25d3343a706f727423276564323a6970b01fee0d343a706f7274fce765"
      "64323a69705f5adae1343a706f7274e3ed6564323a6970d587421e343a70"
      "6f727421b26564323a69708de2b14a343a706f72741c2f6564323a6970c1"
      "4dddc9343a706f7274cbb16564323a6970bc8f071f343a706f7274232765"
      "64323a6970567ffdee343a706f72740d166564323a69705bb7cfc7343a70"
      "6f7274c2c56564323a69705f54c395343a706f72745c1b6564323a69702f"
      "c997bf343a706f7274c4916564323a697005e444d3343a706f7274742865"
      "64323a697053592168343a706f727423276564323a6970b8925f75343a70"
      "6f727438096564323a69700e846ab1343a706f7274202e6564323a69703a"
      "03ef71343a706f727447f96564323a6970b070e0b5343a706f7274de3165"
      "64323a6970542bec71343a706f7274356e6564323a69704d691ef0343a70"
      "6f7274eb596564323a69706da790b6343a706f72748dac6564323a6970d5"
      "151a8b343a706f72741ae96564323a6970415d0b08343a706f7274e61e65"
      "64323a69705214f6c2343a706f7274edd76564323a69700255d090343a70"
      "6f72741ae16564323a6970b49270d8343a706f72741ae96564323a697056"
      "e956b1343a706f7274c8d56564323a697005e465f3343a706f72741ae165"
      "64323a697025bb58a9343a706f7274c8d56564323a697056909add343a70"
      "6f72741ae96564323a697053b5c7e3343a706f727493f56564323a69705d"
      "292600343a706f7274e47f6564323a697063e1c9c1343a706f7274232765"
      "64323a69704c676372343a706f72741ae96564323a69705ff4ac32343a70"
      "6f7274f49d6564323a69700533221e343a706f72741ae16564323a69704f"
      "6de96f343a706f72741ae96564323a6970bc246ae4343a706f72741ae965"
      "64323a69703265c5f0343a706f727448b36564323a69704ec8053a343a70"
      "6f7274ead96564323a69700e2ae2f5343a706f72741ae96564323a6970d4"
      "f7d4f1343a706f7274fa5a6564323a69705e1737cf343a706f72741ae165"
      "64323a69702e07d06d343a706f72741ae96564323a6970586c04ab343a70"
      "6f7274c4916564323a69705f9a1185343a706f72741ae16564323a697059"
      "4b8f31343a706f72741ae96564323a6970253b27a3343a706f7274c8d565"
      "64323a697049ac3dbc343a706f72741ae16564323a69705d8daf79343a70"
      "6f7274ffff6564323a697018f04d8d343a706f72743a766564323a69705c"
      "6fa02a343a706f7274c4916564323a6970585fe155343a706f727421ad65"
      "64323a697025c18d0a343a706f72743cd66564323a6970b0dd8a75343a70"
      "6f7274040b6564323a6970c151f9a3343a706f72741ae16564323a69704c"
      "55439e343a706f727421ad6564323a69705b79536c343a706f7274c8d565"
      "64323a6970b2a4b8f1343a706f727423276564323a69705ac3bfd1343a70"
      "6f7274b9f46564323a69705fb0b533343a706f7274c8d56564323a697063"
      "e4d905343a706f7274613b6564323a69702e09e27c343a706f72741ae165"
      "64323a69705100021c343a706f72742d566564323a6970add4caf8343a70"
      "6f7274c8f66564323a69704c7730be343a706f7274cf9c6564323a697005"
      "bd57b7343a706f7274e81f6564323a6970c47e1604343a706f7274a19965"
      "64323a69704f780800343a706f7274c8d86564323a6970d9a2dec5343a70"
      "6f72741ae16564323a6970b40ec745343a706f72741ae16564323a6970d5"
      "77781e343a706f7274c8d56564323a6970795c23d7343a706f72741ae165"
      "64323a6970740ecb36343a706f727466166564323a6970768ca972343a70"
      "6f72741ae96564323a6970961f4cfc343a706f72741ae96564323a69704f"
      "8bb70d343a706f7274af166564323a69705d4e8290343a706f7274d0d865"
      "64323a697005bdb781343a706f7274b7336564323a69704f7a60b1343a70"
      "6f7274e0b06564323a69705b9490df343a706f727421af6564323a69705c"
      "8f297c343a706f7274a75c6564323a6970bd0d97b9343a706f7274784d65"
      "64323a6970cb198b8e343a706f72741ae96564323a697089ba514d343a70"
      "6f72741ae46564323a69705f328a86343a706f72741ae16564323a69705e"
      "179de1343a706f7274bfad6564323a69705ed2336a343a706f72741ae165"
      "64323a697060f018fe343a706f727469646564323a69706d59ba72343a70"
      "6f727486d96564323a6970737f2377343a706f72741ae96564323a6970d4"
      "e9ac22343a706f7274a8cc6564323a697047baebbd343a706f7274c8d565"
      "64323a69703ecdf8b0343a706f7274870a6564323a697058e77387343a70"
      "6f727496b86564323a6970a3ac4b5f343a706f72741beb6564323a6970a0"
      "28336e343a706f727499b96564323a697089ba514d343a706f72741ae265"
      "64323a697068ab7112343a706f7274413e6564323a697034d542cf343a70"
      "6f72741fa66564323a697025773dde343a706f72741ae96564323a6970c2"
      "b7a807343a706f727490226564323a6970804492d7343a706f7274cca765"
      "64323a6970d16bc487343a706f727421ad6564323a6970546acaaa343a70"
      "6f72741ae96564323a6970053300b9343a706f72743c886564323a69705c"
      "3f11f5343a706f727492ed6564323a69703d4b4253343a706f7274fc9165"
      "64323a69702e3b0ddc343a706f7274bccc6564323a69702ec7ebf3343a70"
      "6f72741ae16564323a697057fbbced343a706f7274612a6564323a697018"
      "5075b0343a706f7274af806564323a6970b038e50c343a706f727482dd65"
      "64323a6970ae35c56a343a706f72741ae96564323a69703ed2a79c343a70"
      "6f7274d71f6564323a69706d58fc97343a706f7274c8d56564323a697032"
      "4fd019343a706f72741ae16564323a6970b2a4f782343a706f72749bf165"
      "64323a69706d5b0226343a706f7274cd2e6564323a69702e002815343a70"
      "6f727415166564323a697059d22c1d343a706f7274abe06564323a697082"
      "ccf8c7343a706f72744ce36564323a6970dc860c32343a706f72741ae165"
      "64323a69702eadbb44343a706f7274b6e96564323a6970506268a1343a70"
      "6f72749ece6564323a69705eaf33eb343a706f72741ae26564323a697052"
      "029d9b343a706f72741ae26564323a697002287881343a706f727442f265"
      "64323a6970beb3bce8343a706f727488626564323a697043a9c652343a70"
      "6f72741ae16564323a69702ef20ce3343a706f72741f1d6564323a697005"
      "40bda6343a706f7274308f6564323a697076f03527343a706f72741ae965"
      "64323a69705f1cfe23343a706f7274c3506564323a69704e2e1168343a70"
      "6f727421c56564323a697050702ea7343a706f7274fc596564323a697056"
      "5ef28b343a706f7274ec806564323a6970477f37e0343a706f7274d09c65"
      "64323a697068f0662b343a706f7274d3206564323a697044e01b0a343a70"
      "6f7274c4916564323a69705b9dbbb4343a706f72748c736564323a69704d"
      "a3185a343a706f7274fde96564323a697055abea3e343a706f72741ae165"
      "64323a6970afb75622343a706f72741ae96564323a6970505e3790343a70"
      "6f7274c8d56564323a69704f8be0c6343a706f727451956564323a697057"
      "005d48343a706f72741ae16564323a6970ada59696343a706f7274a0cd65"
      "64323a69709750261f343a706f7274c8d56564323a697077abee6f343a70"
      "6f727489726564323a69702efb6af1343a706f72741ae16564323a69706f"
      "639c0b343a706f7274d26c6564323a697059419232343a706f7274c8d565"
      "64323a69708ba7439c343a706f7274feb56564323a69704968afbb343a70"
      "6f727462576564323a6970b026a846343a706f7274c8d56564323a697025"
      "9fe2f4343a706f7274c1236564323a697097e05c83343a706f7274c8d565"
      "64323a6970578c27e6343a706f72744c4b6564323a69705f05de8d343a70"
      "6f72741ae16564323a697057f00ed3343a706f7274c7986564323a697063"
      "f2d5e7343a706f727496116564323a69704966d313343a706f7274a58365"
      "64323a697005a75aff343a706f72741ae26564323a69705bcdee0a343a70"
      "6f7274a3e86564323a6970b9930de4343a706f7274c3316564323a69705f"
      "5e6526343a706f7274a1ab6564323a69707779c48a343a706f7274640465"
      "64323a69701f1ced89343a706f72741ae16564323a697033ae2daa343a70"
      "6f7274fcb46564323a69706d406476343a706f7274bf696564323a697055"
      "41018d343a706f7274ba7e6564323a6970b92dc3c7343a706f72746d6465"
      "64323a6970d57f5759343a706f727499736564323a697097e639f8343a70"
      "6f72741ae96564323a6970d57f1fd5343a706f727477896564323a69705d"
      "5ccb27343a706f7274ee6a6564323a6970adae9639343a706f7274ad8865"
      "64323a697053fd8253343a706f727463446564323a6970450e12ff343a70"
      "6f72741ae26564323a6970af82648c343a706f72741ae16564323a697005"
      "4f5b13343a706f7274f4b96564323a697025bc52dc343a706f727445cc65"
      "64323a69708f891dc4343a706f7274a5066564323a6970d9d2b3e6343a70"
      "6f72744d3c6564323a69704f832b45343a706f727450906564323a6970b3"
      "236f83343a706f72743a1e6564323a6970dbe479db343a706f727450d165"
      "64323a69702bf1efa0343a706f72741eca6564323a697057753e9e343a70"
      "6f72740b0d6564323a69705492e0da343a706f72741ae16564323a69704d"
      "523780343a706f7274af436564323a6970c1537403343a706f72741ae965"
      "64323a6970c9f6ee32343a706f7274c16e6564323a69703df4625b343a70"
      "6f72741ae16564323a69705d97eb94343a706f72741ae96564323a697053"
      "db934e343a706f72743a146564323a69705fbb9de7343a706f7274065965"
      "64323a6970adee1890343a706f72741ae26564323a69704e6be967343a70"
      "6f7274c8d56564323a6970546bcacc343a706f7274c8d56564323a6970ad"
      "e0f540343a706f72741ae26564323a69705d2e5890343a706f72741ae165"
      "64323a697057fe9e22343a706f7274634e6564323a69706dbd02c4343a70"
      "6f7274f45c6564323a6970598d42c4343a706f72744eba6564323a697051"
      "41add4343a706f72747d706564323a69708be4e60a343a706f727474bb65"
      "64323a697075663f4a343a706f7274c4916564323a697025699d53343a70"
      "6f72745fe16564323a697079d67b8c343a706f7274d6f96564323a69705b"
      "9964d4343a706f7274c16c6564323a697043f62186343a706f7274c8d565"
      "64323a697055da905a343a706f7274a7e36564323a69703131edd4343a70"
      "6f7274b2726564323a6970a39ed807343a706f72741ae96564323a6970b9"
      "08dc33343a706f72741ae16564323a6970459d44e1343a706f72741ae465"
      "64323a69705d49db76343a706f7274b6ce6564323a6970bd189c33343a70"
      "6f727427106564323a69704fb2f3d1343a706f72749e346564323a6970c3"
      "36a880343a706f7274c0696564323a69704fb4202f343a706f72742faa65"
      "64323a69705f1f52f7343a706f7274cdeb6564323a69706fc6b0ea343a70"
      "6f72743c2e6564323a6970bf64a60a343a706f7274a93c6564323a69704d"
      "6f8a6b343a706f7274cff56564323a69706db6396f343a706f7274645065"
      "64323a6970bb413ed8343a706f7274dad96564323a69705f5ac1b5343a70"
      "6f7274e57b6564323a6970b3b0fb7e343a706f7274c4916564323a6970d5"
      "59db76343a706f727485866564323a69706d56796f343a706f7274bbae65"
      "64323a6970dfdf9b84343a706f72741ae16564323a697055cf0a6b343a70"
      "6f72741ae96564323a69703e4c18f3343a706f7274c21d6564323a697051"
      "208d5b343a706f72741ae16564323a6970ae69372e343a706f7274fabb65"
      "64323a6970ba7d8c22343a706f7274c16a6564323a697025e8965c343a70"
      "6f72741ae16564323a697057cb6203343a706f72744ca46564323a6970dd"
      "96b7c6343a706f727486156564323a69705f1a0bda343a706f7274fb3665"
      "64323a697057a378de343a706f72741ae16564323a69709996b7c6343a70"
      "6f72741ae96564323a6970c7542a40343a706f72741ae16564323a69704a"
      "8bbd4c343a706f7274d8656564323a69706eff855c343a706f7274353b65"
      "64323a69704b9c9ce2343a706f72742ceb6564323a6970175b8e7c343a70"
      "6f72741ae26564323a69705db91119343a706f7274c3276564323a697059"
      "2c0fee343a706f72747f846564323a69702534bbaf343a706f7274565165"
      "64323a69704de8965c343a706f7274570b6564323a6970ae5d76a8343a70"
      "6f7274c8d56564323a69702964394c343a706f727449776564323a6970bb"
      "02ff9b343a706f7274cf226564323a69705f998215343a706f727490d465"
      "64323a69705bb4a8a3343a706f7274ae216564323a6970abe338de343a70"
      "6f727404006564323a6970c501a7bb343a706f7274c4916564323a6970af"
      "089158343a706f72741ae96564323a697069e1e558343a706f72741ae165"
      "64323a69706965fcea343a706f72743a826564323a69704f42b9be343a70"
      "6f7274835f6564323a6970bd7bd9b6343a706f7274e3126564323a69706d"
      "926ab2343a706f7274963a6564323a69706d9b19b6343a706f7274e30865"
      "64323a6970b33662ef343a706f7274a6116564323a697063e90430343a70"
      "6f72741ae76564323a697063e90430343a706f72741ae16564323a69704d"
      "8b59b6343a706f727475276564323a6970510e05a2343a706f7274c5b765"
      "64323a69701f27a5aa343a706f7274fd3e6564323a697063e90430343a70"
      "6f72741ae56564323a69705985382c343a706f7274b9476564323a69705f"
      "54ec65343a706f727475576564323a697057f84196343a706f7274d9c865"
      "64323a69705fff1950343a706f7274c3de6564323a69701fd37c18343a70"
      "6f7274d58c6564323a6970d3e0f5d7343a706f72741ae16564323a697055"
      "ff4004343a706f727446936564323a6970c323f51e343a706f7274f3b065"
      "64323a697051cd0e8e343a706f72749e346564323a69706f033c18343a70"
      "6f727424906564323a69706d6e0741343a706f72748c5b6564323a6970c3"
      "2035d7343a706f72741ae96564323a697053db8859343a706f7274300165"
      "64323a697053db8859343a706f72740c006564323a697033ae144d343a70"
      "6f72741ae16564323a6970538e690f343a706f7274c8d66564323a69704d"
      "8811e0343a706f72741ae96564323a69707a79daaa343a706f72741ae165"
      "64323a69705de90bc7343a706f72741ae16564323a69705f4f10fd343a70"
      "6f72741ae96564323a6970672f405d343a706f72744f8b6564323a697029"
      "6066db343a706f727427206564323a6970556955df343a706f72741ae965"
      "64323a69705fa6200c343a706f727417026564323a6970ad50e6db343a70"
      "6f7274e5196564323a69701f0f8a77343a706f7274c8d56564323a69705f"
      "25a9c3343a706f7274d2496564323a697077c4ee86343a706f7274232765"
      "64323a69705d73af14343a706f727423276564323a6970b5d26851343a70"
      "6f727496a46564323a69705f25a9c3343a706f72741ae26564323a6970b1"
      "8b5b55343a706f7274c54d6564323a6970bd126851343a706f7274598d65"
      "64323a697031922851343a706f727412716564323a6970bb7ddd82343a70"
      "6f7274056c6564323a69700512e851343a706f72745cc66564323a697060"
      "2f90fa343a706f7274ba676564323a697036d183c7343a706f72741aec65"
      "64323a6970b2a2d6e1343a706f72741ae16564323a6970c61b52b5343a70"
      "6f72741ae36564323a697085825bc0343a706f72741ae16564323a697036"
      "4dda17343a706f72741aec6564323a69705344ee86343a706f7274232765"
      "64323a697036c2548b343a706f72741ae16564323a69705b79a48a343a70"
      "6f7274820f6564323a697036d183c7343a706f72741aec6564323a69703e"
      "d26e37343a706f72741ae46564323a69702f58207e343a706f7274276665"
      "64323a6970bb7ddd82343a706f727430d86564323a697046be4708343a70"
      "6f72741ae1656565";

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

        sp::UinStaticArray<dht::IdContact, 256> nodes;

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

  krpc::ParseContext pctx(dht, in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}
