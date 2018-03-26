#include "bencode.h"
#include "dht_interface.h"
#include "krpc.h"
#include "gtest/gtest.h"
#include <encode/hex.h>

using namespace dht;

TEST(krpc2Test, test) {

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

TEST(krpc2Test, test2) {
  sp::UinStaticArray<Contact, 128> nodes;
  char raw[] = "5:nodesl6:123456e";
  sp::Buffer in((unsigned char *)raw, sizeof(raw));
  in.length = sizeof(raw);
  ASSERT_TRUE(bencode::d::peers(in, "nodes", nodes));
}
