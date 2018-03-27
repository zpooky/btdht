#include "bencode.h"
#include "dht_interface.h"
#include "krpc.h"
#include "gtest/gtest.h"
#include <encode/hex.h>

using namespace dht;

TEST(krpc2Test, get_peers) {

  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);

  Infohash h;
  auto sctx = insert(dht.searches, h);

  unsigned char raw[1024];
  sp::Buffer out(raw);
  Contact peer;

  unsigned char raw_in[1024];
  sp::Buffer in(raw_in);
  {
    const char *hex =
        "64323a6970363a51e8520d0719313a7264323a696432303a4e651186274b7818f2a47a"
        "27c376761a75fc4cff353a6e6f6465733230383a4e64a430e9928885e3f7a1a19af194"
        "65bb8a13ad59a9cfe4b35d4e648fe1c71133770ba42f86fc65820adf6c7971d4b29aae"
        "47074e64f849f1f1bbe9ebb3a6db3c870c3e99245e52d58f58dc04114e64db1dfb9452"
        "5da3ebe28d1b1a1c59757c22eb5f56f8d42e704e643f49f1f1bbe9ebb3a6db3c870c3e"
        "99245e52492b474bb7504e640c5bc5f1491016b0cd3fd2279a2d600e266754d5245dc8"
        "d54e6474a76ccd0dd9bd62f4714fe40d87265031b4bfb1ec6ffae54e6452df19575e31"
        "b9402c41807696d9ea73cb8b505c10c4e605353a746f6b656e32303aaacdd4c0c9be36"
        "fcabdbede174fa99b0319e578f363a76616c7565736c363a02da00e6bea1363a524fcd"
        "5488bb363a538b530a0412363ab2568f8206b9363aa8a74f8b1ae1363a500a475efa69"
        "363a33b36535a266363abc1877907769363ad5953ea42677363a5cf0bae7e423363ac5"
        "c8e02e7ce0363a6e36ee9a1ae1363a4deec7527db5363a7664e74365f9363a59a5bc0a"
        "7350363a59d467d0da0b363ab5b052febbb2363a4f0066f50405363a5b92b70aa60936"
        "3a4d8abb3d4443363a97304e6bb446363a5b964fa3b0d4363a25e4e8e522b6363ac33c"
        "48463ad4363a2ec455b1498c363a80416fd371b5363a972d31023ea3363ad5ca44c138"
        "14363a5a9c25dad2eb363a412336812777363a6d5c817285da363acdd9f3142d42363a"
        "5e15a7fd0405363a538b530a0410363a5d2c6614ed5d363abddabd061b37363ab04d89"
        "feee8f363a567bb55f2771363ac39e5db60408363a0223a13d6588363a1fd1d888c568"
        "363a9718f550d9fd363a4def1f3acd86363a555cef673f1b363a2e95d50d527e363abc"
        "1ab550a0bf363a567d35909ad1363a25a81900cd7c363a5d883800e187363ac903b986"
        "f408363a538b5a0297d5363a5d8a72d9c68f363a2e239c0b2a5d363ac3b249e13ba636"
        "3a5d24a61f3417363ab3d56a08e79e363a51b417315d06363a543abb649caf363a9740"
        "2aac4163363ac95f2c726063363a51d52d7fcb41363ac3c7f6064604363a9736aba452"
        "49363a6ca832595d68363a256a6dd0880f363a592bc37eea02363ab111415130b4363a"
        "05acecdd6703363a5865500e55fe363a5a32d0585062363a7c08df9d41f3363a5774b2"
        "6d1ac3363a050ca7a73c306565313a74343a6268f1ea313a76343a5554ad46313a7931"
        "3a7265";
    in.length = sizeof(raw_in);
    ASSERT_TRUE(hex::decode(hex, raw_in, in.length));
    {
      sp::Buffer copy(in);
      sp::bencode_print(copy);
    }
  }

  Module m;
  get_peers::setup(m);

  auto f = [&](krpc::ParseContext &pctx) -> bool {

    dht::MessageContext ctx{dht, pctx, out, peer};
    assert(std::strcmp(pctx.msg_type, "r") == 0);

    assert(bencode::d::value(pctx.decoder, "r"));

    return m.response(ctx, sctx->ctx);
    // return true;
  };

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}

TEST(krpc2Test, get_peers2) {
  sp::UinStaticArray<Contact, 256> nodes;
  char raw[] = "5:nodesl6:123456e";
  sp::Buffer in((unsigned char *)raw, sizeof(raw));
  in.length = sizeof(raw);
  ASSERT_TRUE(bencode::d::peers(in, "nodes", nodes));
}

TEST(krpc2Test, find_node) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);

  unsigned char raw[1024];
  sp::Buffer out(raw);
  Contact peer;

  unsigned char raw_in[1024];
  sp::Buffer in(raw_in);
  {
    const char hex[] = "64313a7264323a696432303a61c58ef52d9f57f311e954e50a2eea9"
                       "7b2a30ca4323a6970"
                       "343a51e8520d353a6e6f6465733230383a61c5187c2acd7c756d4fb"
                       "db034afe3e3cb0992"
                       "745518b8b4c8d560a2c9fc1e7377d33891183965283e4be6c2578d5"
                       "12315413b3d6439c5"
                       "d2658e07471916c9b0b8a8a0dea7d525c5d93dc3641ae965454ca45"
                       "95f382702aecefda2"
                       "b40afe1659532658e649482fab6f69798cf4949c79f57a72a5a9f28"
                       "92130b19398484f0c"
                       "daeded6e2b5711c5af78cd9b0dbd1ed99cf6ec184828de5b7957bdd"
                       "7476a83b658a95c56"
                       "6924010f2ec0591ac0027cec6d3ed2c83dd75b6b966a083538b4722"
                       "a5bf404641af694a5"
                       "a5354a5beaf801272465313a74343a6569cbef313a76343a4c54000"
                       "f313a79313a7265";
    in.length = sizeof(raw_in);
    ASSERT_TRUE(hex::decode(hex, raw_in, in.length));
    {
      sp::Buffer copy(in);
      sp::bencode_print(copy);
    }
  }

  Module m;
  find_node::setup(m);

  auto f = [&](krpc::ParseContext &pctx) -> bool {

    dht::MessageContext ctx{dht, pctx, out, peer};
    assert(std::strcmp(pctx.msg_type, "r") == 0);

    assert(bencode::d::value(pctx.decoder, "r"));

    return m.response(ctx, nullptr);
  };
  dht.active_searches++;

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}

TEST(krpc2Test, find_node2) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);

  unsigned char raw[1024];
  sp::Buffer out(raw);
  Contact peer;

  unsigned char raw_in[1024];
  sp::Buffer in(raw_in);
  {

    const char hex[] =
        "64313a7264323a696432303a61c58ef52d9f57f311e954e50a2eea97b2a30ca4323a69"
        "70343a51e8520d353a6e6f6465733230383a61c5187c2acd7c756d4fbdb034afe3e3cb"
        "0992745518b8b4c8d560a2c9fc1e7377d33891183965283e4be6c2578d512315413b3d"
        "6439c5d2658e07471916c9b0b8a8a0dea7d525c5d93dc3641ae965454ca4595f382702"
        "aecefda2b40afe1659532658e649482fab6f69798cf4949c79f57a72a5a9f2892130b1"
        "9398484f0cdaeded6e2b5711c5af78cd9b0dbd1ed99cf6ec184828de5b7957bdd7476a"
        "83b658a95c566924010f2ec0591ac0027cec6d3ed2c83dd75b6b966a083538b4722a5b"
        "f404641af694a5a5354a5beaf801272465313a74343a6569cbef313a76343a4c54000f"
        "313a79313a7265";
    in.length = sizeof(raw_in);
    ASSERT_TRUE(hex::decode(hex, raw_in, in.length));
    {
      sp::Buffer copy(in);
      sp::bencode_print(copy);
    }
  }

  Module m;
  find_node::setup(m);

  auto f = [&](krpc::ParseContext &pctx) -> bool {

    dht::MessageContext ctx{dht, pctx, out, peer};
    assert(std::strcmp(pctx.msg_type, "r") == 0);

    assert(bencode::d::value(pctx.decoder, "r"));

    return m.response(ctx, nullptr);
  };
  dht.active_searches++;

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}

TEST(krpc2Test, ping) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);

  unsigned char raw[1024];
  sp::Buffer out(raw);
  Contact peer;

  unsigned char raw_in[1024];
  sp::Buffer in(raw_in);
  {

    const char hex[] = "64323a6970363a51e8520d2710313a7264323a696432303a676dcdf"
                       "37a35fcb3a3478beca810cb10b1179f4e313a706931303030306565"
                       "313a74343a656a10d3313a76343a4c540100313a79313a7265";
    in.length = sizeof(raw_in);
    ASSERT_TRUE(hex::decode(hex, raw_in, in.length));
    {
      sp::Buffer copy(in);
      sp::bencode_print(copy);
    }
  }

  Module m;
  ping::setup(m);

  auto f = [&](krpc::ParseContext &pctx) -> bool {

    dht::MessageContext ctx{dht, pctx, out, peer};
    assert(std::strcmp(pctx.msg_type, "r") == 0);

    assert(bencode::d::value(pctx.decoder, "r"));

    return m.response(ctx, nullptr);
  };
  dht.active_searches++;

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}

TEST(krpc2Test, xxx) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);

  unsigned char raw[1024];
  sp::Buffer out(raw);
  Contact peer;

  unsigned char raw_in[1024];
  sp::Buffer in(raw_in);
  {

    const char hex[] =
        "64323a6970363a51e8520d2710313a7264323a696432303ad21c8aa750c01a84954d63"
        "8248ba380efebc0b1f353a6e6f6465733230383afad6db7f51f9be60df5bfa7047c006"
        "7f2377ff1cde63bd1b1ae1fee6fa771f5f24b9fa2de243518404abf644f8fd5f254fdf"
        "7f0af037119ad3819ebe92e221c72f81807f189971f3c372884549bbf2aee88fcdbcba"
        "2fe1782245af10c34fd2bee8814f941eba612af2ef1b82ddfdc69d83554875cd9ce537"
        "81e3dd0263cb1634be1ae8c55014db730f7f1789b420c607ddfb6ab445f4b25b3d4fe2"
        "f4eee4bb604902811780a8768c511517436950916bd918e246fccce23c8e1c9c2e1201"
        "0530efda45e474e57c84692967f9e50f1ae165313a74343a05450000313a79313a726"
        "5";
    in.length = sizeof(raw_in);
    ASSERT_TRUE(hex::decode(hex, raw_in, in.length));
    {
      sp::Buffer copy(in);
      sp::bencode_print(copy);
    }
  }

  Module m;
  find_node::setup(m);

  auto f = [&](krpc::ParseContext &pctx) -> bool {

    dht::MessageContext ctx{dht, pctx, out, peer};
    assert(std::strcmp(pctx.msg_type, "r") == 0);

    assert(bencode::d::value(pctx.decoder, "r"));

    return m.response(ctx, nullptr);
  };
  dht.active_searches++;

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}

TEST(krpc2Test, get_peers3) {

  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);

  unsigned char raw[1024];
  sp::Buffer out(raw);
  Contact peer;

  unsigned char raw_in[1024 * 4];
  sp::Buffer in(raw_in);
  {
    const char hex[] =
        // "64313a7264323a696432303a4e64127dd96ba8437ed1d3bb402fc9bd05c2ba31323a69"
        // "70343a51e8520d313a6e35303a4d4943524f534f4654204f66666963652050524f2050"
        // "6c75732032303136207631362e302e343236362e313030332052544d353a6e6f646573"
        // "3230383a4e649b6a4ae0dff359a39d5e995fdb3675428197538b92621b654e649b50a1"
        // "7922e86652c6e06b7915639fece607c90d94b5048a4e649f171fa2e4e23b77f3a38751"
        // "158619feacba6d7b9b1fc8d54e64ec00690e1dd14490720849770b1f84aa60e3998562"
        // "171ae94e64f849f1f1bbe9ebb3a6db3c870c3e99245e52d58f58dc04114e64f22e2b97"
        // "9e56b33f258710961511a58195284b98194e68064e64c201566d52d4a94f60d27da812"
        // "c6c1c15a2725d2f08963b44e64c3a97d5f0fb0a8c529a1c1533413d3f98ba8433d455a"
        // "1ae9353a746f6b656e32303afaff7f79574ed20f79314aa19233e8418e3745e6363a76"
        // "616c7565736c363a8bc2d5041ae1363a55ba0f79e09a363a75c0f846a750363a056bf5"
        // "b3adb4363ac5ce8f28c008363a4eb0fbf7c8d5363abb7297c3adbc363aa90093822abb"
        // "363ab51a84678862363ac87825b5b7ee363abdca380e3a7b363abc35d86a49d5363a29"
        // "ec8bc97e38363a567a35b8576a363a772855c2c0e8363a76945cad2c4c363ac4b43d27"
        // "e06e363a0233892cd4df363aab6085f155ff363abf25cefd6a46363abb271fdf2a1736"
        // "3a5b3080ba92d7363a3ea374e9eee7363ac41de45b3425363a53081b3a2a8d363abc1b"
        // "1a8f711b363aba00e01a216b363abe8eab064c0c363a056b6c5e9f5a363a675738165d"
        // "d7363a6eebce64386d363a73e7df43c345363aac7020b35006363a0592c7915900363a"
        // "c94c888e3acb363acbbcf4f32e83363acbc0d1341ae1363a567991a84b81363a567938"
        // "324b1a363a5ebb58d83ee6363a6367f470afcb363a57753e4b2fb8363a0232b5a548eb"
        // "363a516e0a6bd17e363a92c4228f1ae1363a58518ae4f87f363a74c18ec2ab83363a52"
        // "00f6059a98363a4e02735d5bb0363aa0da603834dc363a5679161832d8363a293ce8d2"
        // "3d74363a02317d70becd363ac7747259f792363a5774b215930d363a4f61f322551436"
        // "3a83a1e5eb3d24363a2966408ba5f4363ac41cf6ccb1f9363a25e4e4b01ec9363abf23"
        // "2c3c7a6a363abcadc5cc606e363a5f9190eada92363a023388e02ce1363a5e1d88277d"
        // "1f363a2d725214be5e363aa100ff0cd0fd363ab020165d5ff7363a29df855eaa13363a"
        // "5d670977f095363a6d5df37934e6363a973e59155b85363ac5d3d853d823363ac5d3d8"
        // "b9f790363a2be7d03f4e75363a4fb790abc0c1363a5d6164df273f363a33df026c3129"
        // "363abd2f39bdccb6363a4cad550f2327363aba587c062327363a298fff5fbe9c363ab9"
        // "712094611d363a67578f119bcb363a671a555eea46363a3ab6c087d3f9363a58fcc6ab"
        // "e136363abc45c0d99307363a67c04b0f9d35363aba561f26e5c4363a566244bc4a3636"
        // "3a29b6c3c15672363a3dd31f642614363ab816da1c533b363ab5bc7f7532d6363a1fd8"
        // "5c687b77363a4c70930c411f363abd472ec538c5363a4fb28b13ea12363a7c08df9d41"
        // "f36565313a74343a6276f6be313a76343a5554ab14313a79313a7265";

        // "64313a7264323a696432303a4e648aaf55761f997b6bb270d7a513c8d2cf6fb9353a6e"
        // "6f6465733230383a4e64aee66d1e6e2a2d7bb29963ed849198128ee75f4e4e76945f4e"
        // "64af34394726c7e96e85dfaef7e0cd3f4815946dce9e42d5704e64a3c4c75a7d6eb9ee"
        // "bef39b3f30b453621ffa55c0bce0092b4e64a749f1f1bbe9ebb3a6db3c870c3e99245e"
        // "52b336fa377c804e64bbb02e38458d55baa684b86ebca3b254ea49495bc16e1ae14e64"
        // "b800e298182107c82e2429d394f9bc0c4fb15dab416e7f784e64bea1609b8f6a89618f"
        // "2ff645ff8b5de44a6eb9b2326a7a264e64bf6e2bc24c910eca9b40c9b5e68d80ac2239"
        // "b14b416e086e363a76616c7565736c363a8d88fb2a7d76363ac9d276e5c926363a2e67"
        // "b1026a73363a272aad569497363ac4d95bcf6a50363a48ad8a86becc363a3eeef82b7a"
        // "0e363a2924a2559682363a74492de52977363ad963e44da348363a43d70afaef15363a"
        // "4df31ae239cd363a5183f1f53d30363abd3ac548a98a363ad86e72f2827d363abb6dde"
        // "0afb06363a59b1b3c753bb363ab457f218e358363a5f42ce35d565363aa9ffc52fc008"
        // "363a4fa9168097f2363a5f5fa2383d31363a5e63d8f14158363a5355deadadda363a57"
        // "d9f1fe388c363a02e1467fd833363aab07aa5d820b363aa7fa48fe313c363a5bbb7162"
        // "9a44363a4649708d1ae1363a4d8a44564ff8363ab15e9f2f70bf363a98aced5e886236"
        // "3abb157233a19a363a56b69859e403363abd3ed23d5266363a7aa5415657c9363a2a6e"
        // "82b02327363a252f75405701363adad094322b80363a59ebcdacc000363a57e0e2fee1"
        // "77363a4e85021f27d6363a5c7386b6e1df363ab19e6799e98f363ac84908422fa4363a"
        // "5e41e978c100363a6e36ec03cd2a363abd7fdcd13d65363abb545f7e53466565313a74"
        // "343a6468d087313a76353a417a07d0a1313a79313a7265";

        "64313a7264323a696432303a629a57a2325faeeda065ea8c04b7bc01260ccdde353a6e"
        "6f6465733230383a629a5f6aafe901520730e7791010aaabf1169d1a1f0f8a77c8d562"
        "9a5db13c63cb6f9bbd94caa782cbd4bdebd2495b4d1d82b193629a5f5c69b338738f3c"
        "e2db60f190a9ab2ebb0e1f84ae860800629a5f58e37abadb0746f3a4971641bc87e154"
        "fe696268518fb2629a58cf134ca83b7f57090c252af23837c50981cbdd9d8232b1629a"
        "5addb2ed0750399c181d7665eb2bd78943cc86f9587e65e5629a5f963ab5fc33d8b999"
        "a2fb4169338049cd69598b5b55b7ab629a58da003a6371c54f135c59640c5e7b511d61"
        "cbcd1d82760165313a74343a6475156e313a79313a7265";
    in.length = sizeof(raw_in);
    ASSERT_TRUE(hex::decode(hex, raw_in, in.length));
    {
      sp::Buffer copy(in);
      sp::bencode_print(copy);
    }
  }

  Module m;
  get_peers::setup(m);

  auto f = [&](krpc::ParseContext &pctx) -> bool {

    dht::MessageContext ctx{dht, pctx, out, peer};
    assert(std::strcmp(pctx.msg_type, "r") == 0);

    assert(bencode::d::value(pctx.decoder, "r"));

    return m.response(ctx, nullptr);
  };
  dht.active_searches++;

  krpc::ParseContext pctx(in);
  ASSERT_TRUE(krpc::d::krpc(pctx, f));
}
