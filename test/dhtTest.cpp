#include "timeout.h"
#include "util.h"
#include "gtest/gtest.h"
#include <dht.h>
#include <hash/fnv.h>
#include <list>
#include <map/HashSetProbing.h>
#include <prng/util.h>
#include <set>
#include <util/assert.h>

using namespace dht;

template <typename T>
static void
insert(std::list<T> &collection, const T &v) noexcept {
  collection.push_back(v);
}

template <typename T>
static void
insert(std::set<T> &collection, const T &v) noexcept {
  collection.insert(v);
}

static Node *
random_insert(dht::DHT &dht) {
  dht::Node n;
  fill(dht.random, n.id.id);

  dht::Node *res = dht::insert(dht, n);
  // ASSERT_TRUE(res);
  if (res) {
    assertx(res->timeout_next);
    assertx(res->timeout_priv);
    // printf("i%zu\n", i);
  }

  const dht::Node *find_res = dht::find_contact(dht, n.id);
  if (res) {
    assertx(find_res);
    assertx(find_res->id == n.id);
  } else {
    assertx(!find_res);
  }

  dht::Node *buf[Bucket::K]{nullptr};
  dht::multiple_closest(dht, n.id, buf);
  dht::Node **it = buf;
  bool found = false;
  // while (*it) {
  for (std::size_t i = 0; i < Bucket::K && it[i]; ++i) {
    if (it[i]->id == n.id) {
      found = true;
      break;
    }
  }

  if (res) {
    // assertx(found);
  } else {
    assertx(!found);
  }

  return res;
}

static void
random_insert(dht::DHT &dht, std::size_t cap) {
  for (std::size_t i = 0; i < cap; ++i) {
    // printf("%zu.\n", i);
    random_insert(dht);
  }
}

static void
assert_empty(const Node &contact) {
  ASSERT_FALSE(is_valid(contact));
  ASSERT_EQ(contact.timeout_next, nullptr);
  ASSERT_EQ(contact.timeout_priv, nullptr);

  ASSERT_FALSE(dht::is_valid(contact.id));
  ASSERT_EQ(contact.contact.ip.ipv4, Ipv4(0));
  ASSERT_EQ(contact.contact.port, Port(0));

  ASSERT_EQ(contact.remote_activity, Timestamp(0));
  ASSERT_EQ(contact.req_sent, Timestamp(0));

  ASSERT_EQ(contact.outstanding, std::uint8_t(0));
  ASSERT_EQ(contact.good, true);
}

#if 0
static void
ads(dht::DHT &dht, dht::RoutingTable *parent, std::size_t it) {
  // printf("%d\n", parent->type);
  ASSERT_TRUE(parent->type == dht::NodeType::LEAF);

  {
    for (std::size_t i = 0; i < dht::Bucket::K; ++i) {
      dht::Node &contact = parent->bucket.contacts[i];
      assert_empty(contact);
    }
  }

  std::size_t idx = 0;
  dht::split(dht, parent, idx);
  ASSERT_TRUE(parent->type == dht::NodeType::NODE);
  ASSERT_TRUE(parent->node.lower != nullptr);
  ASSERT_TRUE(parent->node.higher != nullptr);

  if (it > 0) {
    ads(dht, parent->node.higher, it - 1);

    ads(dht, parent->node.lower, it - 1);
  }
}

TEST(dhtTest, split) {
  fd sock(-1);
  Contact c(0, 0);
  dht::DHT dht(sock, c);

  auto parent = new dht::RoutingTable;
  ads(dht, parent, 10);
}

#endif

static void
assert_prefix(const NodeId &id, const NodeId &self, std::size_t idx) {
  for (std::size_t i = 0; i < idx; ++i) {
    bool id_bit = bit(id.id, i);
    bool self_bit = bit(self.id, i);
    ASSERT_EQ(id_bit, self_bit);
  }
}

template <std::size_t size>
static Node *
find(Node *(&buf)[size], const NodeId &search) {
  for (std::size_t i = 0; i < size; ++i) {
    // printf("find: %p\n", buf[i]);
    if (buf[i]) {
      if (buf[i]->id == search) {
        return buf[i];
      }
    }
  }

  return nullptr;
}

static Node *
find(Bucket &bucket, const NodeId &search) {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    Node &c = bucket.contacts[i];
    if (is_valid(c)) {
      if (c.id == search)
        return &c;
    }
  }

  return nullptr;
}

static void
verify_routing(DHT &dht, std::size_t &nodes, std::size_t &contacts) {
  const NodeId &self_id = dht.id;

  assertx(dht.root);
  std::size_t idx = dht.root->depth;

  RoutingTable *root = dht.root;

Lstart:
  if (root) {
    ++nodes;
    printf("%zu.  ", idx);
    print_id(self_id, idx, "\033[91m");
    auto it = root;
    while (it) {
      Bucket &bucket = it->bucket;
      for (std::size_t i = 0; i < Bucket::K; ++i) {
        Node &c = bucket.contacts[i];
        if (is_valid(c)) {
          printf("-");
          print_id(c.id, idx, "\033[92m");
          ++contacts;
          assert_prefix(c.id, self_id, idx);
        } else {
          assert_empty(c);
        }
      }
      it = it->next;
    } // while
    root = root->in_tree;
    ++idx;
    goto Lstart;
  }
}

static void
self_should_be_last(DHT &dht) {
  RoutingTable *root = dht.root;
Lstart:
  if (root) {
    Bucket &bucket = root->bucket;
    if (root->in_tree == nullptr) {
      // selfID should be in the last node routing table
      dht.id.id[19] = sp::byte(~dht.id.id[19]);
      Node *res = find(bucket, dht.id);
      ASSERT_FALSE(res == nullptr);
    }
    root = root->in_tree;
    goto Lstart;
  }
}

// static std::size_t
// equal(const NodeId &id, const Key &cmp) noexcept {
//   std::size_t i = 0;
//   for (; i < NodeId::bits; ++i) {
//     if (bit(id.id, i) != bit(cmp, i)) {
//       return i;
//     }
//   }
//   return i;
// }

// assert_present(DHT &dht, const NodeId &current) {
static void
assert_present(dht::DHT &dht, const Node &current) {
  Node *buff[Bucket::K * 8] = {nullptr};
  {
    dht::multiple_closest(dht, current.id, buff);

    Node *search = find(buff, current.id);
    assertx(search != nullptr);
    ASSERT_EQ(search->id, current.id);
  }

  {
    dht::Node *res = dht::find_contact(dht, current.id);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->id, current.id);
    ASSERT_TRUE(res->timeout_next);
    ASSERT_TRUE(res->timeout_priv);

    timeout::unlink(dht, res);
    ASSERT_FALSE(res->timeout_next);
    ASSERT_FALSE(res->timeout_priv);

    timeout::append_all(dht, res);
    ASSERT_TRUE(res->timeout_next);
    ASSERT_TRUE(res->timeout_priv);
  }
}

#define insert_self(dht)                                                       \
  do {                                                                         \
    dht::Node self;                                                            \
    std::memcpy(self.id.id, dht.id.id, sizeof(dht.id.id));                     \
    dht.id.id[19] = sp::byte(~dht.id.id[19]);                                  \
    auto *res = dht::insert(dht, self);                                        \
    ASSERT_TRUE(res);                                                          \
  } while (0)

static void
assert_count(DHT &dht) {
  std::size_t v_nodes = 0;
  std::size_t v_contacts = 0;
  verify_routing(dht, v_nodes, v_contacts);

  printf("added routing nodes %zu\n", v_nodes);
  printf("added contacts: %zu\n", v_contacts);
}

TEST(dhtTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);

  insert_self(dht);

  //
  random_insert(dht, 1024 * 1024 * 4);
  assert_count(dht);

  // printf("==============\n");
  // printf("dht: ");
  // print_id(dht.id, 18, "");
  dht::debug_for_each(dht, nullptr,
                      [](void *, dht::DHT &self, const auto &current) {
                        assert_present(self, current);
                      });

  self_should_be_last(dht);
}

template <typename F>
static void
for_each(Node *const start, F f) noexcept {
  Node *it = start;
Lstart:
  if (it) {
    f(it);
    it = it->timeout_next;
    if (it != start) {
      goto Lstart;
    }
  }
}

TEST(dhtTest, test_link) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);

  random_insert(dht, 1024);

  assert_count(dht);

  // printf("==============\n");
  // printf("dht: ");
  // print_id(dht.id, 18, "");
  dht::debug_for_each(dht, nullptr,
                      [](void *, dht::DHT &self, const auto &current) {
                        assert_present(self, current);
                      });
}

static bool
is_unique(std::list<NodeId> &l) {
  for (auto it = l.begin(); it != l.end(); it++) {
    // printf("NodeId: ");
    // print_hex(*it);
    for (auto lit = it; ++lit != l.end();) {
      // printf("cmp: ");
      // print_hex(*lit);
      if (*it == *lit) {
        return false;
      }
    }
  }
  return true;
}

TEST(dhtTest, test_append) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);

  for (std::size_t i = 1; i <= 50; ++i) {
    // int i = 1;
  Lretry:
    printf("%zu.\n", i);
    Node *ins = random_insert(dht);
    if (ins == nullptr) {
      goto Lretry;
    }

    {
      dht::Node *res = dht::find_contact(dht, ins->id);
      ASSERT_TRUE(res);
      ASSERT_EQ(res->id, ins->id);
      ASSERT_TRUE(res->timeout_next);
      ASSERT_TRUE(res->timeout_priv);

      {
        std::list<dht::NodeId> unique;

        std::size_t number = 0;
        for_each(dht.timeout_node, [&](Node *n) { //
          ++number;
          insert(unique, n->id);
          // printf("NodeId: ");
          // print_hex(n->id);
        });
        // printf("ASSERT_EQ(number[%zu], i[%zu])\n", number, i);
        ASSERT_EQ(number, i);
        ASSERT_EQ(unique.size(), i);
        ASSERT_TRUE(is_unique(unique));
      }

      timeout::unlink(dht, res);
      ASSERT_FALSE(res->timeout_next);
      ASSERT_FALSE(res->timeout_priv);
      {
        std::list<dht::NodeId> unique;

        std::size_t number = 0;
        for_each(dht.timeout_node, [&](Node *n) { //
          ++number;
          insert(unique, n->id);
        });
        ASSERT_EQ(number, i - 1);
        ASSERT_EQ(unique.size(), i - 1);
        ASSERT_TRUE(is_unique(unique));
      }

      {
        timeout::append_all(dht, res);
        ASSERT_TRUE(res->timeout_next);
        ASSERT_TRUE(res->timeout_priv);
      }

      {
        std::list<dht::NodeId> unique;

        std::size_t number = 0;
        for_each(dht.timeout_node, [&](Node *n) { //
          ++number;
          insert(unique, n->id);
        });
        ASSERT_EQ(number, i);
        ASSERT_EQ(unique.size(), i);
        ASSERT_TRUE(is_unique(unique));
      }
    }
  }
}

TEST(dhtTest, test_node_id_strict) {
  fd sock(-1);
  prng::xorshift32 r(1);
  for (uint32_t i = 0; i < 10000; ++i) {
    Ip ip(i);
    Contact c(ip, 0);
    dht::DHT dht(sock, sock, c, r, sp::now());
    init(dht);
    ASSERT_TRUE(is_strict(ip, dht.id));
  }
}

TEST(dhtTest, test_node_id_not_strict) {
  fd sock(-1);
  prng::xorshift32 r(1);
  dht::NodeId id;
  for (uint32_t i = 0; i < 500000; ++i) {
    Ip ip(i);
    Contact c(ip, 0);
    fill(r, id.id);
    ASSERT_FALSE(is_strict(ip, id));
  }
}

TEST(dhtTest, test_full) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);
  const char *id_hex = "B161DDEAF70A2485C32789B965CE62753B11CEFF";
  printf("%zu\n", std::strlen(id_hex));
  ASSERT_TRUE(from_hex(dht.id, id_hex));

  constexpr std::size_t sz = 127;
  const char *buf[sz] = {"B33DA63EF24B92B888C1C794C2E5284D4595A987",
                         "B2890449F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B542F9B280FABB24D6A3EDC02800DCAFCA948F83",
                         "B49CF91478964D2030171434FAE464598C6F3D2D",
                         "B7B1100BD6EE30FC50E17220645A27E89956A03D",
                         "B6B00E8174B428F3FBC2461F0CA82C26725C5067",
                         "B9A6A5C746CAF368CA007C7BF378699735A83239",
                         "B8D5357702D7E8392C53CBC9121E33749E0CF44F",
                         "B15B7CE541583C797DB65452B64E8ED82A1D18AA",
                         "B116ED870C3E99245E0D1C06B747DEB3124DC8DB",
                         "B1F4FD49F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B1A56A5413DCA0678E9924EEFF7547A3C995A920",
                         "B0459CD6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B02D6D49F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B0F82D35ED7C66865FDC65570D15EB24FD82594A",
                         "B09898D2565A107E2875D895929055A7822BE400",
                         "B1752DD6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B1450C33118BA8FE5068DB88C3BB9C9F73601001",
                         "B1445A18F815B6FDF6B63CA1E1C545E443ACFF15",
                         "B14A5C525F022A1CCE51143AFBB29EA019265ACB",
                         "B1D292BB2FA012D7E97C98392CC3DA70D132B2F8",
                         "B56FACF04AFD4C16C01BB1ED7694BAB5B2610E44",
                         "B7C79E4F5499BE9F7F08DF3709B24FDB7A8A8570",
                         "B682F30007991E4F2F2A989E4BFEE4A2C91C01F5",
                         "B34CB8405D4B3505ECCD5C9DAB1130591601688C",
                         "B353EFE3CDF902F76C414FEB7DB7858BA98469DE",
                         "B353E2FDBB1BDFD7512E664250B0A7722F0BEF83",
                         "B353B63C870C3E99245E0D1C06B747DEB3124DA6",
                         "B353A7052272B6C5D4B80FF71768331338949E30",
                         "B358F1672A057241C0F7AC6025E643BA1F989D14",
                         "B35B3FE3AE5BDBB54C868A579654549831AD1523",
                         "B35AE43803CCC25D0B319551F63D46C2A1DC85B6",
                         "B17B86D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
                         "B1DB7412812A3F060D8D40C2DC5E5817471D58CC",
                         "B19329D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B065770280E97628F31C7985E56D4BFEF4160F13",
                         "B01BECEBB3A6DB3C870C3E99245E0D1C06B747BB",
                         "B0EF67E9EBB3A6DB3C870C3E99245E0D1C06B7F1",
                         "B0885BD6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B378C1E314CE4608CE78E2F5059A235A571F6BA5",
                         "B34596D4849A1D962D2667D6BECEF1EBFD0E2E51",
                         "B35A3F7D90D64BCE04147E19069A5950F5B9A932",
                         "B35CDB08403E807807E0F258064FAC58B764309F",
                         "B35CF16B6651655BD77CAC71167AC23EC083D143",
                         "B35C73987E65944110A7606151ABE9EF3C3026F7",
                         "B3A19F0E95CD0615BAB45074498449223A9BC62D",
                         "B3AC11D46E83B08164091D45F3C17E93A00F6016",
                         "B3FA5D49F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B2042449F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B566845239A1312EB33953575CA2A0719367728A",
                         "B45A7549F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B6526449F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B165AFFFC8A6263E4190615669CCDE3BDE770282",
                         "B165176F4749F81EC095E883EE41BCBDEB57CD53",
                         "B166EC69A9FE70BA2F435FF6A2BBCA7EEA7A03F3",
                         "B16C7F317881A9F5C8DBB9F08D0A98EE0D1CC04E",
                         "B170B77A40F31A2E5FB35DECA43128D6B8C98ED6",
                         "B17449DF893C4243B10563A96B5923790894799F",
                         "B17BB54BF77910E9B1CADE7A532E1ACF72B32689",
                         "B17A7B6103C3FE872AB915954EA781F776842465",
                         "B160D042E6FF213751D32AE1C2F6864A604F457A",
                         "B162C44550AA99877C8DF88D3E0D6D982294835F",
                         "B1670594B3BF9E591CBB238EA89114DBA106EABE",
                         "B16F70DB60C5E7D795F68B8044928DB7A06EFA89",
                         "B1768A4BED7EDF0BE303A9C1B4A50995952577FD",
                         "B17DD4444543ABB0B4719E27FF4E3EA051107B6D",
                         "B17C2E53F8A65DC564AC54E5505285EABE1D574B",
                         "B1528E3669B12B6C08B78D8FDFB49D350C5C0448",
                         "B134308CD1C2CFB9689B6B06537E670E384B6B18",
                         "B118F348F02CC9392B0915DA3942675542E9068F",
                         "B1FA9CF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
                         "B1DA34BBE9EBB3A6DB3C870C3E99245E0D1C06F1",
                         "B1BD61D5AEBCF880E0C43CDA9D70D4848C14CD56",
                         "B1950C0C3EA6A5706961FB1CA04DCC2D1B0A4463",
                         "B13E503123F19777286A5D79152A9EFDBDB270B5",
                         "B1B6BAEBB3A6DB3C870C3E99245E0D1C06B747BB",
                         "B0716649F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B0C2BB6DD9ED5D6AACAAF5A79AD24F861421C9D4",
                         "B3556E1D3D84269150415514D8BAF93D5C697021",
                         "B39D17F649ECCFDE4858E3001168B2A6F7B77899",
                         "B254767DEFEDA35FC531C187077DEE136B79BD86",
                         "B2CE68E96A385006A7E23175C6ADE0D63DDAC42E",
                         "B16ED649F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B1443A8A719DEE19B36C74E7151EAB27D8C083E0",
                         "B133EF6ACBEE601920F09116A01216118EE9C94B",
                         "B10E438B7D068032DBA0A705DCFE7104E7F2FEC8",
                         "B1EEFA691F796A420844E3D520B627F49C296C00",
                         "B1C3E565F18F029FE23DCCEC8593C88B3714E7A2",
                         "B1B6C306B747DEB3124DC843BB8BA61F035A7D0D",
                         "B199E190561E4025E898111D2D09E9DD43602A7B",
                         "B17BE3D12ADEF6C31038028E1C0FD168FC652B1C",
                         "B15E34EFFDD9F80C85A27F936C94BB6BE953FD9F",
                         "B126D849F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B1324454B09E3ED3AB8C1A92B4DF1FCB71AFD3A2",
                         "B10275D3B77B19F6DEA332F6A940F02711E49D0B",
                         "B1164D153766C7C59827166A3CCAC1E97D0F1B81",
                         "B15117474F29B196D84304653D02C92070174D89",
                         "B12E0BA5C2BEC8E06327E8AABE25F992FAD7F117",
                         "B12E3CF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
                         "B119B349F1F1BBE9EBB3A6DB3C870C3E99245E52",
                         "B118F4E4BF0A601F6384496AD8EF9A1B908966B8",
                         "B1E03419701AFEDB18937CE2BCBDF8BE78D60AB7",
                         "B1FA1109F4C9B4F9C5254C2ECBCD2ADBE3D216A6",
                         "B1CC46D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B017EADA5329716607A561CC8C41F26BE24C952B",
                         "B0E74DCE8F62133617C14DBB2CD74E329E4306C1",
                         "B18B5E3479E8A67DA1940E5108A0A0171240FBA8",
                         "B1531A57281A3BD1B42DD3031EEF0C8F1D2BC114",
                         "B030BA7631076E04F46B992051DDB3AB1BF2832A",
                         "B1BCB5D465830D7C1748D3AD1ABC844784A32C56",
                         "B0153FC68B2E1BC5ABBDE63A03CBBA0701E18B6D",
                         "B0E967C2E1B5B2396625FA3FEBF8C75A94DF4105",
                         "B16BEEEFE9DBDA0CD87AEF2E88657BE2A5A9285E",
                         "B159BAD6AE529049F1F1BBE9EBB3A6DB3C870CE1",
                         "B13F7947895EFDFAE6293C9E177AD953918882FD",
                         "B114DD40D35FD963F65BE2E80AB7A3D09987B42F",
                         "B1E4DD034A1BED7D6951961DD9D70406DF9D2AE4",
                         "B1BCABA87609CACBE769D17EB874719AB0EC3505",
                         "B18F49EBCB3548C619EA882CB8DC89E4A227C98F",
                         "B17D3A4E726C1BF9547AE88CDD4D10EADDD79272",
                         "B1428C5467EC187252580E4764A773FFC4E6C6D2",
                         "B136F618F8F361EA2D4D4B1A84AC5D46F72C96D0",
                         "B106718CE3FEB90E705AD85AD1ABD2ECE1680D1B",
                         "B1FDC6163AEB499D0F55370B7F3D33AFBE356CE5",
                         "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
                         "B1A9D16BEC94C2175A3BD05737435089765E1B16"};
  for (std::size_t i = 0; i < sz; ++i) {
    dht::Node n;
    ASSERT_TRUE(dht::from_hex(n.id, buf[i]));
    bool res = insert(dht, n);
    ASSERT_TRUE(res);
    auto out = find_contact(dht, n.id);
    ASSERT_TRUE(out);
    ASSERT_EQ(out->id, n.id);
  }

  for (std::size_t i = 0; i < sz; ++i) {
    dht::NodeId n;
    ASSERT_TRUE(dht::from_hex(n, buf[i]));
    auto out = find_contact(dht, n);
    ASSERT_TRUE(out);
    ASSERT_EQ(out->id, n);
  }

  printf("%zd depth\n", dht.root->depth);
  {
    dht::Node n;
    n.id = dht.id;
    do {
      // printf("%d\n", n.id.id[19]);
      n.id.id[19]++;
      insert(dht, n);
    } while (n.id.id[19] != dht.id.id[19]);
  }
}

TEST(dhtTest, test_first_full) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);
  const char *id_hex = "FF61DDEAF70A2485C32789B965CE62753B11CEFF";
  ASSERT_TRUE(from_hex(dht.id, id_hex));

  for (std::size_t i = 0; i < 1024 * 1; ++i) {
    dht::Node n;
    fill(dht.random, n.id.id);
    n.id.id[0] = 0;
    insert(dht, n);
  }
  ASSERT_EQ(debug_levels(dht), 1);

  printf("%u\n", nodes_total(dht));
  {
    dht::Node n;
    n.id = dht.id;
    n.id.id[19]++;
    ASSERT_TRUE(insert(dht, n));
  }
  ASSERT_EQ(debug_levels(dht), 2);
}

struct RankNodeId {
  dht::DHT *self;
  NodeId id;
  RankNodeId()
      : self{nullptr}
      , id{} {
  }

  RankNodeId(DHT &s, const NodeId &o)
      : self{&s}
      , id{o} {
  }

  bool
  operator==(const RankNodeId &o) const {
    return id == o.id;
  }

  bool
  operator==(const NodeId &o) const {
    return id == o;
  }

  bool
  operator>(const RankNodeId &o) const noexcept {
    auto f = rank(self->id, id.id);
    auto s = rank(self->id, o.id.id);
    return f > s;
  }
};

namespace sp {
template <>
struct Hasher<RankNodeId> {
  std::uint64_t
  operator()(const NodeId &n) const noexcept {
    return fnv_1a::encode64(n.id, sizeof(n.id));
  }
};
} // namespace sp

TEST(dhtTest, test_self_rand) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);
  heap::StaticMaxBinary<RankNodeId, 1024> heap;

  // std::size_t count = 0;

  for (std::size_t i = 1; i < 1024; ++i) {
    sp::HashSetProbing<NodeId> set;
    std::size_t routing_capacity =
        std::max(dht.root_limit * Bucket::K, length(dht.rt_reuse) * Bucket::K);

    for (std::size_t x = 0; x < routing_capacity; ++x) {

      auto strt = uniform_dist(dht.random, 0, std::min(i, NodeId::bits));

      Node node;
      node.id = dht.id;
      for (std::size_t a = strt; a < NodeId::bits; ++a) {
        node.id.set_bit(a, uniform_bool(dht.random));
      }
      // printf("%zu. %s\n", ++count, to_hex(node.id));

      insert(dht, node);
      if (!lookup(set, node.id)) {
        insert(set, node.id);
        insert_eager(heap, RankNodeId(dht, node.id));
      }
    } // for

    std::size_t kx = 0;
    RankNodeId out;
    while (take_head(heap, out)) {
      auto r4nk = rank(dht.id, out.id.id);
      if (kx < routing_capacity) {
        printf("%zu .%zu, [%zu]\n", i, kx, r4nk);
        ++kx;
        Node *fres = find_contact(dht, out.id);
        assertx(fres);
        assertx(fres->id == out.id);
      }
    }

  } // for
}

TEST(dhtTest, test_assert_fail) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);
  const char *id_hex = "B161DD0EAF70A2485C32789B965CE602753B11CE";
  ASSERT_TRUE(from_hex(dht.id, id_hex));

  constexpr std::size_t sz = 65;
  const char *buf[sz] = {
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B045123115ABD8784F149690536AB41A4BFAD89F",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B7A814F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B90D27D65ADF384196A31655EA426FCABEE0C1D8",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "8276D253D55442EF7370CD0EF3A01929EAA7BD9E",
      "A537253A5CA42116E8DA0F6132557906D774C821",
      "B49E9A52E48DA49E9EC31A3728689E8DC01579CB",
      "BA92EC7AA43EE872C605E7B43147163BB829170A",
      "BA1BC03EAF4839134CDF8CDF78837E426F4B6677",
      "BA1D53531E5B3D7D715582D7CDC65B9EB81827D6",
      "BA1D99EB6594D454DFEE7C9106E0660CA83F49F4",
      "BA1D91D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "ADDEBACF3E502BD7C9015BBA08CBB0E54519F44A",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2B4B049F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B16543C425E42C94450E0CC1FF8D656A46FDAB21",
      "B042B2F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B09CA00C3E99245E0D1C06B747DEB3124DC8433C",
      "B342F1DBA4ED9AA732AE4DEA9A232FC3D36EA7FB",
      "B3CA55E138A013540F4E3C81FA6E659EC79151B2",
      "B23F9372F12F38064F0CF9F33978719668DDAF49",
      "B2806BFE948A220268D3D9F9E48E7F25EA70C348",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B137D137BF375AEB3A2A216599F7CFD174D2FC11",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B1674EF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B1378AE9EBB3A6DB3C870C3E99245E0D1C06B7F1",
      "B10CB05E63B87A1DEB0DF680824A788BE8BF656C",
      "B1F672311B80FA1D29A3DE545B925DA74230F902",
      "B1D7DEA342EFBC8B6B14AAE52C9FE1FA012A6080",
      "B1BD61D5AEBCF880E0C43CDA9D70D4848C14CD56",
      "7FFE6AF50E345100A17751083208CBC2346D5929",
      "B1F3A9F27823DE23E22F1E7ADA8B756F4A8FB142",
      "B050CE79E15D48147247FBCCBA0D06CB616C12DC",
      "B2FCE60BEA1D800F2DA7278AA9A3220EFE43A86A",
      "B56F48BE47F9BAC2A6FB92A6E0F0087AA2D15E12",
      "B4AD5BC89AADDF99C7128652B8C90B15D7AA2EC8",
      "B7CD3BFDA807287EB66EE0FC494F2D5CA21F3632",
      "B7A48FF09842E872ACCF66C20F504225477222F0",
      "B605D67998A1C6E20801414C0E26A9F180C829FF",
  };

  for (std::size_t i = 0; i < sz; ++i) {
    dht::Node n;
    ASSERT_TRUE(dht::from_hex(n.id, buf[i]));
    bool res = insert(dht, n);
    ASSERT_TRUE(res);
    auto out = find_contact(dht, n.id);
    ASSERT_TRUE(out);
    ASSERT_EQ(out->id, n.id);
  }
  printf("\n");
}

TEST(dhtTest, test_assert_fail2) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, sock, c, r, sp::now());
  dht::init(dht);
  const char *id_hex = "B161DD0EAF70A2485C32789B965CE602753B11CE";
  ASSERT_TRUE(from_hex(dht.id, id_hex));

  constexpr std::size_t sz = 916;
  const char *buf[sz] = {
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B175EC01E6064ABAE7EF8B56EDD42DB7F79ED692",
      "B17C6901741E671EF3FC172FEDFC89B7448FEC5E",
      "B110FB99531175F68FB23B51D6C1B523B3311C5C",
      "B1FC95DD90E6C7EAE1C9D5D51B656929322A3BD9",
      "B04804248C9320111F3AA5EC83BD450E86B90C3B",
      "B05596D7FFAD03A934E87ABDB5292735EB7DE38B",
      "B019DA2D9481F79E8CCAD29259E0119B8B847E09",
      "B0ADB7A2950852692AD1480A965253996BA7AAD8",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B14B14291961DBF1E85690B37336A4FEE73C36BD",
      "B1057D46946A9831696A61BC573196C0A4813E1C",
      "B1FDC6163AEB499D0F55370B7F3D33AFBE356CE5",
      "B186C2A80FFB42FA181F54895AC754E57916A7A3",
      "B05AAF0208D8F23398C2DCE363D2C74C3E0002C1",
      "B019DA2D9481F79E8CCAD29259E0119B8B847E09",
      "B0CB1DA3054574F22414A74911214015C831BEFB",
      "B0BC553D2464F2315B59C4AB4D2E9BD3C9FCF5DE",
      "B164F9533B383FD25EF58CF4CFCE95A5D9C77486",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1FDC6163AEB499D0F55370B7F3D33AFBE356CE5",
      "B18DDBA562CF4D6F6AC67CEB1990D300C9F39789",
      "B07C0DD11547107252D9887D8DAE3968F7041E3F",
      "B0005D0155A3895A8A82B0D77F9BBEDE1714D191",
      "B0C4F1C3D157A454F0C72F5E0B07FC90E954A3C3",
      "B08BACD4407FA92A9B991FBE3ED7325F0AA767CB",
      "B161B6B3A6DB3C870C3E99245E0D1C06B747DEE9",
      "B13CD384BD6943934CB329E95B86FE66287F6487",
      "B1F672311B80FA1D29A3DE545B925DA74230F902",
      "B18E9517B3806A2F07F7EF1FF431781F6BB18B82",
      "B06493E3D4973A0777386189B867E9A56A04111A",
      "B01254803604FD591F2F07D90682370F51194610",
      "B0F13E945DE0EF6DA5D5E9317F1402A2380A054E",
      "B09342F466BB12A8F4967E44461B0F243836E036",
      "B107574FDABF2F770889191E243F1709F16E0BDB",
      "B1A9B01164A9EFF3E1A5EE60C6204D6413B22BCF",
      "B05B2E6F5C48A4DCFD28EC4C002E2BE7286541C5",
      "B0CF43D99A8B45ECA584D467118837E1A4631C26",
      "B36E9D2FDBE6E2941E7921549BCE469A0328E989",
      "B3A9542683EA2A3DFE7F9E903C606B13323CDBC4",
      "B20C9196C6EC4AED3F4B0C4B79DDD5B048B7B952",
      "B2A5B0432C9274436582106653F15908362C388B",
      "B15CD7C41CA908C449AFC03492837AC0F1F8A278",
      "B1B5EC4BCCAADF07AB19AD89A00D9E8F3F53993C",
      "B042B2F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B0BD6CB4BAFE7E0DF939927435D935D0835CFE7C",
      "B3619249F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B218C26445BD13AC89D0E1C9C77B7803377FC117",
      "B2F70D20FC62425028C540CBF3F9AA49EEFC0C43",
      "B5577A064D056423020355CADF398FD0F2CCAB37",
      "B16C9113EEA945FA6BDEE3A80F25B38F9422512C",
      "B17D4C238A8E08793AB054C30E52498B03D6D87F",
      "B12D87D48ABA5FDAC893AD4C2F751251719C13DE",
      "B10D3B1EA04E98EACF3F86236504E62EFB5CCFDF",
      "B1179B70B2F8609E1A220FBBDCEF324D716838F9",
      "B1EC047B00C69C46229982049C16C7C6A999D9F9",
      "B1A52A71D9E6C092A988781F9D637AF9ADC21FAC",
      "B18534D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B04003140603D13D81A5066CC883A68BEB46D708",
      "B0DB3A12E8C217A1D571E2EF1D45A339D788C7C0",
      "B3409549F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B3E9C6F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B20B52F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B2C7D304B41EB2074A3D744BED6C35D15638D4A3",
      "B141779903F69B321756FB6EAEF4E3E858A792A5",
      "B14A05BE598A5A8D7DBEE57273D01CC0CF13CC32",
      "B14A4D911C768B8197F063EC8A3F0A174906FC09",
      "B163A264718BDC4D0F0BF627CAD807D8823D8FF2",
      "B1720DD387C6865E0C813108EA9CACAD93001E9E",
      "B1551005A0E4D95CAD5F6BFE0A995CDD79F5DC5C",
      "B176F3861BBE5648565074C190D813B5C1F0EDEC",
      "B14CB5E1611E65EA06805D12BD485E9BA81FB29A",
      "B160B2CF652C348DC41C208A6E9018D85A5E23D7",
      "B16D2F7AB187E9C43A7EDB762DAA9D6CA838C17D",
      "B15117474F29B196D84304653D02C92070174D89",
      "B15079DE2AF3B6B6C6F208C774B1F70F8FCB1FA7",
      "B1538991390974AB01444F9A3532B8192EA4EFA8",
      "B15271C1937205B247600A9A3398651B747FCBC2",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B18227EDE62F657F2EA07D393A0ACFD8E4B70CED",
      "B16680DB3C870C3E99245E0D1C06B747DEB312B3",
      "B17C9BC1CE94ED937D5E535C76B4CDBD2B52F7D7",
      "B1438FF2752B651CDFE63BFAFEB3D58D775AD77D",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B121B9E11E8D7EDD5892917D3509445BF10CDA32",
      "B136F3A338580B6355202D883F53991FCEA7B9A9",
      "B10CB05E63B87A1DEB0DF680824A788BE8BF656C",
      "B11776DF1F389303DFB61AA2A0D4E12AADFCB87A",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B12F908C9810FF9A43CDCF57C75059BFBD1C2781",
      "B1C44E8259FF39FA9E6B879B53C16B7E41732038",
      "B1B6D2A6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B07C1C2F4D27EBD6E303BE6A3902F75DCF3AA82E",
      "B037DABBE9EBB3A6DB3C870C3E99245E0D1C06F1",
      "B0C3FF0A71AFD673966228B866AD5C8883D37E9D",
      "B0AEDFF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1683F71A9C884BACDA45C2CC41354B320702D9E",
      "B174A8A6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B176FD7EF2EF86D6DCE8B1E83B14CD636E0DF57A",
      "B17E12E9534DD980975F9049C7D91C99E12A91DD",
      "B17E3EC119E4B6CB4D9454FC03A7BA9797991991",
      "B141633BAFB2E33FFF0D0DE7C8043421C852D98A",
      "B1428C5467EC187252580E4764A773FFC4E6C6D2",
      "B1581642A1D015BE865135AA19D6325383AFA9AF",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B175EC01E6064ABAE7EF8B56EDD42DB7F79ED692",
      "B17C6901741E671EF3FC172FEDFC89B7448FEC5E",
      "B110FB99531175F68FB23B51D6C1B523B3311C5C",
      "B1FC95DD90E6C7EAE1C9D5D51B656929322A3BD9",
      "B04804248C9320111F3AA5EC83BD450E86B90C3B",
      "B05596D7FFAD03A934E87ABDB5292735EB7DE38B",
      "B019DA2D9481F79E8CCAD29259E0119B8B847E09",
      "B0ADB7A2950852692AD1480A965253996BA7AAD8",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1E7EF275E26F7B9C2EC0E7CA405C399D9043196",
      "B07AE8D0890646C118AA9F778DB1D45B8D9BF8F2",
      "B0BEC0CFD3902D48D38F75E6D91D2AE5C0F72BF4",
      "B35AEE180B9E0FEAC32C8379947E82DC8A7B28F3",
      "B3EFABAE407136D309FE52A048A73765A3884691",
      "B2181CDB3C870C3E99245E0D1C06B747DEB312B3",
      "B2CE0DA6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B14B14291961DBF1E85690B37336A4FEE73C36BD",
      "B1057D46946A9831696A61BC573196C0A4813E1C",
      "B1FDC6163AEB499D0F55370B7F3D33AFBE356CE5",
      "B186C2A80FFB42FA181F54895AC754E57916A7A3",
      "B05AAF0208D8F23398C2DCE363D2C74C3E0002C1",
      "B019DA2D9481F79E8CCAD29259E0119B8B847E09",
      "B0CB1DA3054574F22414A74911214015C831BEFB",
      "B0BC553D2464F2315B59C4AB4D2E9BD3C9FCF5DE",
      "AC54F1B03C18DAEC23E2EC4E6E41015AB58C4EDA",
      "B1A447629323A55764ED8987BD927128913AD305",
      "B367D9A288ED478F073C9EA0E368C235DDF67190",
      "B36B5EF2BF5647A0190943E71FD7E273D1C30EAB",
      "B5565B5E14DDED8326DB6E62176BF1F665A36270",
      "B5564BAC58829F4E5B34F46CA77F80F29D7938E2",
      "B4938349F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B7E3CA0735B293DDB222F927C4395D85A067DB2C",
      "B68BE983B35792E97F820D7665E7C527FBCE1F33",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B16461CB38C273D2BBB9A908E64EA4F41CDFEC2E",
      "B17D3A4E726C1BF9547AE88CDD4D10EADDD79272",
      "B14EF70C68C626C6F35B7A91179029987E7E15CD",
      "B1520E62BD9407CFE2A547689AF97AAAC81B8143",
      "B1246700F046AE1C1823C43EAA1B1C3F73D6194F",
      "B133EF6ACBEE601920F09116A01216118EE9C94B",
      "B105A0F7310CF40F9DE7D38C0B6D4CD1EEC8FEA3",
      "B1109765F6C6EF47E2C23292957D356F1003EF55",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B10343357BA6EFD89CD8F2B671F46EE6462B783F",
      "B103680E4B6E83D2B06F95F84C8F525E3E675ABE",
      "B1117A134584514674DC746D2AC1A46FC0FD344B",
      "B1ECA1D897888BAC44ED82F082DE2CFF6745FE3E",
      "B1D30DF625C87DF480F76DEA8668D4C506F92AEC",
      "B1D954D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1DA3757DC1736618C82DF965C3054360BEAE041",
      "B190F7481F843E5172FC5E0C4741F90D9C17D1AE",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B066293F9FE01F3628B77694DC9A15E89714FFF6",
      "B0D097A91A70C63EE483DDA98B8614151DC7F4E4",
      "B0D3EDB3FCBE83AEC710F2EF39E6F609B082F1AF",
      "B0D3E92A0C9F4837E2F2CF1D3ABFEFBA984A185A",
      "B0D29124CA13E1B1E6CA366BEAA8CABA32EEACA5",
      "B3699D80216364615A31C8DFB1567E1BABA500DC",
      "B392A5823C1A4F72361001E19C06E32A2B583473",
      "B271AC54A09442CD9D2AAFF3FBF8367FE66E0365",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B1610CBB7B3BAC2601BE5BEB51FF26A0C22D14E5",
      "B1630988BC95DA0A33806F8F12D3E619E92E7143",
      "B1675ED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1645ED9DDA7F77E1532526EB5D5E371E502BF33",
      "B1647A7C68C82CA7B5C1E6C7A630121F4C77D3B2",
      "B163FD6234728AFCE714AEADBD1370709956644B",
      "B166EC69A9FE70BA2F435FF6A2BBCA7EEA7A03F3",
      "B161AFA4761F4E5A7772CE6626114E0964463F6A",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B16A84430449CD1588901EC1B7835528BFEB2375",
      "B1630988BC95DA0A33806F8F12D3E619E92E7143",
      "B16B7F979B0B72FDD7C551F43BF0A145B8B6327D",
      "B16E39D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B16B75D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B16034BA4107726B3404370F40D287F8087C264C",
      "B16BC69E54D559E373ECF4B071FBEBA642B9648A",
      "B164F9533B383FD25EF58CF4CFCE95A5D9C77486",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B12F908C9810FF9A43CDCF57C75059BFBD1C2781",
      "B1C44E8259FF39FA9E6B879B53C16B7E41732038",
      "B1B6D2A6DB3C870C3E99245E0D1C06B747DEB3EB",
      "B07C1C2F4D27EBD6E303BE6A3902F75DCF3AA82E",
      "B037DABBE9EBB3A6DB3C870C3E99245E0D1C06F1",
      "B0C3FF0A71AFD673966228B866AD5C8883D37E9D",
      "B0AEDFF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B168DC407AE0C3F11468F9F6FAF3C64292E1C494",
      "B177F2B267C6C88C133BBEB5F463965A58A994FD",
      "B17D3D7F7CCF7F1284F1B4B9186B5DFE0D362BFB",
      "B17C097191964D537B2ACB318AC02970CBB95689",
      "B15D2A2F4C3BD15525FB6C6F40D3C975D2075724",
      "B1333F0671696CAC07E8A437089C13356CA65FD0",
      "B11B6131ECD4D1950EE0E2D78D1E771C3493D977",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B17817C4D2D4C3E0EE8FEEE4C5D4649895A2BEB4",
      "B176DC901E73E7E8BFD91B588777A0E9ACBAFF64",
      "B16C632DF76A8D85ACE3777F5864FF90A5E60A1F",
      "B12F4C49F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B170956189E6AC2534A6626EE5E0A25E049924F1",
      "B1693C0A5416D5035FDAF913DD919CE2E3064318",
      "B1329563605B8ACB0EDF8C55FFA1C95BF2F1999B",
      "B138E1B908658FDC4AF3F38DD370FEF74B4844D0",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B1720DD387C6865E0C813108EA9CACAD93001E9E",
      "B10C0D2E4507716227A9B2D4DEF65FAC43AC27C9",
      "B110FB99531175F68FB23B51D6C1B523B3311C5C",
      "B11F1F0E74F3349778043A243BF9ABB9C6C7254E",
      "B1DD41F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B1BF07D55C91023016419D9E4971F138F927BBB8",
      "B18CC0C5711C10EE19E81CFA7DA754CDFE5A078E",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1237658B5FE7D7237D68A9A50F3DC186B9DA21B",
      "B136F618F8F361EA2D4D4B1A84AC5D46F72C96D0",
      "B112D3DE9DFB8FD6EDCA0303BD21EB2A87FF112A",
      "B10FCA13034F45444F0A73DA71358049A07B7907",
      "B117E99338EE41F6F1844E4537885A37F5FC680A",
      "B1433351AAA6A285BC49F2949B2F723445DEA5EF",
      "B125900553B561AEBC9AF1F949A98C0E44ED3426",
      "B15ED2EBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B16680DB3C870C3E99245E0D1C06B747DEB312B3",
      "B17C9BC1CE94ED937D5E535C76B4CDBD2B52F7D7",
      "B1438FF2752B651CDFE63BFAFEB3D58D775AD77D",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B121B9E11E8D7EDD5892917D3509445BF10CDA32",
      "B136F3A338580B6355202D883F53991FCEA7B9A9",
      "B10CB05E63B87A1DEB0DF680824A788BE8BF656C",
      "B11776DF1F389303DFB61AA2A0D4E12AADFCB87A",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B07B856253F88E6A703C2A9A0E14CD74792AC6CB",
      "B32836935284A6FADB9C3FF3AEE0AFE8CF7A7A0B",
      "B586048391932EB6D51C5C729768006177985424",
      "B4F78FF0CC42302BD53088F82C32AF3E6A8DA250",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B6BE1DEBB3A6DB3C870C3E99245E0D1C06B747BB",
      "B8653FF505E5C598FAB2A084FF87B7BE1A22614F",
      "B1D70AAECB4BC19CC831C9F44A95965D1F30C6E1",
      "B16B0E00ED9CB52428A63B2436A373596FD91616",
      "B192E15BDD26D64678C37FF6D25C421DDF1D6D72",
      "B19BECD0775E9E07CFFEFA47614DCD4AFC98BAC0",
      "B1713EC79362D82D0775B52D87D216D4DFF080A7",
      "B105BF44DA10EC8BAD3CAC6ED7A367487A2BA27E",
      "B1BC547B40E673F386FBF28865925FD9545B4CA2",
      "B1985DB1257E15173D3A30421D0E7E935481D856",
      "B16C9113EEA945FA6BDEE3A80F25B38F9422512C",
      "B17D4C238A8E08793AB054C30E52498B03D6D87F",
      "B12D87D48ABA5FDAC893AD4C2F751251719C13DE",
      "B10D3B1EA04E98EACF3F86236504E62EFB5CCFDF",
      "B1179B70B2F8609E1A220FBBDCEF324D716838F9",
      "B1EC047B00C69C46229982049C16C7C6A999D9F9",
      "B1A52A71D9E6C092A988781F9D637AF9ADC21FAC",
      "B18534D6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B164F9533B383FD25EF58CF4CFCE95A5D9C77486",
      "B11B18D6FB9314D15A7FF6072E2BD1F5BF0B3A87",
      "B1FDC6163AEB499D0F55370B7F3D33AFBE356CE5",
      "B18DDBA562CF4D6F6AC67CEB1990D300C9F39789",
      "B07C0DD11547107252D9887D8DAE3968F7041E3F",
      "B0005D0155A3895A8A82B0D77F9BBEDE1714D191",
      "B0C4F1C3D157A454F0C72F5E0B07FC90E954A3C3",
      "B08BACD4407FA92A9B991FBE3ED7325F0AA767CB",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B15CD7C41CA908C449AFC03492837AC0F1F8A278",
      "B1B5EC4BCCAADF07AB19AD89A00D9E8F3F53993C",
      "B042B2F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B0BD6CB4BAFE7E0DF939927435D935D0835CFE7C",
      "B3619249F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B218C26445BD13AC89D0E1C9C77B7803377FC117",
      "B2F70D20FC62425028C540CBF3F9AA49EEFC0C43",
      "B5577A064D056423020355CADF398FD0F2CCAB37",
      "B586048391932EB6D51C5C729768006177985424",
      "B145872B310F21BF2B08AD928FFB05370F9A9B1E",
      "B146A030A1CBCAA5CFA459CD55DA7F3CDDD0AA34",
      "B18CC0C5711C10EE19E81CFA7DA754CDFE5A078E",
      "B18CB0D9F02FDE76AEEDA1AD2E6EF6DB54EE8354",
      "B061E8E4E221974A622410C6F58D89EB648D20FC",
      "B0671B672D825C4AF155334964F3E51E10B53B5B",
      "B370C30ED9162F6768D4F74A4AD0576876FA1696",
      "B3514C7ECB2EA96A6BA54CF98B4EB8BEE748ACE6",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B161B6B3A6DB3C870C3E99245E0D1C06B747DEE9",
      "B13CD384BD6943934CB329E95B86FE66287F6487",
      "B1F672311B80FA1D29A3DE545B925DA74230F902",
      "B18E9517B3806A2F07F7EF1FF431781F6BB18B82",
      "B06493E3D4973A0777386189B867E9A56A04111A",
      "B01254803604FD591F2F07D90682370F51194610",
      "B0F13E945DE0EF6DA5D5E9317F1402A2380A054E",
      "B09342F466BB12A8F4967E44461B0F243836E036",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B107574FDABF2F770889191E243F1709F16E0BDB",
      "B1A9B01164A9EFF3E1A5EE60C6204D6413B22BCF",
      "B05B2E6F5C48A4DCFD28EC4C002E2BE7286541C5",
      "B0CF43D99A8B45ECA584D467118837E1A4631C26",
      "B36E9D2FDBE6E2941E7921549BCE469A0328E989",
      "B3A9542683EA2A3DFE7F9E903C606B13323CDBC4",
      "B20C9196C6EC4AED3F4B0C4B79DDD5B048B7B952",
      "B2A5B0432C9274436582106653F15908362C388B",
      "87EBE26AB327FE2364D28434E989BCC884A1E8FB",
      "B19DCA675D15E00EAB6672ED79611CEE43D113AF",
      "B0BE5B101EF13359A8F7BC1D97A8F4D37C3ECE68",
      "B3D3D4B0A8AC278AA083AF8838925EA01A961A55",
      "B44398DB3C870C3E99245E0D1C06B747DEB312B3",
      "B606F21951F0AE54B1DA100E27D0679DFD02F4B6",
      "B6A3AA2C76E1D76AEB0DBB7A8962901D610351ED",
      "B9C94DF1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B873D9EA9E447B0F9B4BB2CB847AA8A024802295",
      "B1C497D8A1DDE22031198501BF6B0F7564DECECA",
      "B1D387811CA3F8683D02D3330958EAE840EFAD78",
      "B1D4C3B8CADA0A072E2CE3BF4CAAB134FCFCCCA9",
      "B1D48D878993BFBC48EB09F179B2F89ECED2D7B8",
      "B1D4AED6AE529049F1F1BBE9EBB3A6DB3C870CE1",
      "B1D41EAAB39A668DC73F7FFA41D6CD89AEB05FBD",
      "B1D43C06AF1575589152CB6D07151711DFCAD0C4",
      "B1D67EDB3C870C3E99245E0D1C06B747DEB312B3",
      "B17FAAEFBD87FC121D62CDB6BB197E37D9C2C1C2",
      "B13F05AED4748CCA3188152870052D711E95E93C",
      "B1F190ADC697CE57C6AC803D307C18B511C8AE2B",
      "B180EAA9A96BD41E9A0D7053526A1F90F0411B8F",
      "B074F83D76841889250C18BD10D263F46CA07B06",
      "B0074CBC9ED1092E9B66F1148F0EB2FB0233388E",
      "B0C0EB085E42529853B05EB96750298F7C895607",
      "B080345B18DADBBECF69AEF5B160262BDCE057C3",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B13FA0F222C9DC26C1C381D24A989BEDEB49E879",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B04003140603D13D81A5066CC883A68BEB46D708",
      "B0DB3A12E8C217A1D571E2EF1D45A339D788C7C0",
      "B3409549F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B3E9C6F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B20B52F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B2C7D304B41EB2074A3D744BED6C35D15638D4A3",
      "B18833049C65CA10CD1920F79D5B8585F973F2F3",
      "B01A11B88F40B70849802C3E02B2042E5883D601",
      "B36D22852A35529484287593FD62E56FB1FE916C",
      "B27E4CC843BB8BA61F035A7D0938251F5DD4CB12",
      "B515E449F1F1BBE9EBB3A6DB3C870C3E99245E52",
      "B43AC0B999428242AFE7E6576582133D0C37C30A",
      "B763FEA5A157E51C0125ECF006CF05A241716B68",
      "B610B03F895EF16D0EE36C28A11E201DCBC20339",
      "B160B2CF652C348DC41C208A6E9018D85A5E23D7",
      "B16D2F7AB187E9C43A7EDB762DAA9D6CA838C17D",
      "B15117474F29B196D84304653D02C92070174D89",
      "B15079DE2AF3B6B6C6F208C774B1F70F8FCB1FA7",
      "B1538991390974AB01444F9A3532B8192EA4EFA8",
      "B15271C1937205B247600A9A3398651B747FCBC2",
      "B158F47237E5464B04AEDEB8373755545494C24C",
      "B18227EDE62F657F2EA07D393A0ACFD8E4B70CED",
      "B1607FF9A9351446BC770DDB014521172B15DFFA",
      "B165C5C32185332EB6098E6DACCA19EA1BABC1A4",
      "B168BF9DF2AB60CC09323E32F45E351468149880",
      "B173C6280B314462CC34BA2CE5301BCFB03D84A9",
      "B17E6802F5E56FF680B0EE567BE69E5AD528AD04",
      "B14CCD9123AA92A459F81DB02C56FBA4C5D9E34B",
      "B153307A0E3775D302463D467C436210CFA0EAE0",
      "B154616E2A0C290592DC15CD1EAC3F6BF5EC4C97",
      "B0ADB7A2950852692AD1480A965253996BA7AAD8",
      "B165E6B8AFA434CC8396F021BD6A35BC589FFC53",
      "B1720DD387C6865E0C813108EA9CACAD93001E9E",
      "B10C0D2E4507716227A9B2D4DEF65FAC43AC27C9",
      "B110FB99531175F68FB23B51D6C1B523B3311C5C",
      "B11F1F0E74F3349778043A243BF9ABB9C6C7254E",
      "B1DD41F1F1BBE9EBB3A6DB3C870C3E99245E0D90",
      "B1BF07D55C91023016419D9E4971F138F927BBB8",
  };

  for (std::size_t i = 0; i < sz; ++i) {
    if (i == 877) {
      printf(".%zu\n", i);
    }
    dht::Node n;
    bool search_find = false;
    ASSERT_TRUE(dht::from_hex(n.id, buf[i]));

    bool res = insert(dht, n);

    Node *search[32]{nullptr};
    multiple_closest(dht, n.id, search);
    Node *it = search[0];
    for (size_t a = 0; a < 32 && it; ++a) {
      it = search[a];
      if (it) {
        if (it->id == n.id) {
          search_find = true;
          break;
        }
      }
    }

    auto out = find_contact(dht, n.id);
    if (res) {
      ASSERT_TRUE(out);
      ASSERT_EQ(out->id, n.id);
      // TODO ASSERT_TRUE(search_find);
    } else {
      ASSERT_FALSE(out);
      ASSERT_TRUE(!search_find);
    }
  }
  printf("\n");
}

// TEST(dhtTest, test2) {
//   fd sock(-1);
//   Contact c(0, 0);
//   dht::DHT dht(sock, c);
//   dht::init(dht);
//   std::list<dht::NodeId> added;
//
//   insert_self(dht, added);
//
//   for (std::size_t i = 20; i < NodeId::bits; ++i) {
//     Node test;
//     std::memcpy(test.id.id, dht.id.id, sizeof(test.id.id));
//     test.id.id[i] = !test.id.id[i];
//
//     {
//       const dht::Node *fres = dht::find_contact(dht, test.id);
//       ASSERT_FALSE(fres);
//     }
//
//     Node *res = dht::insert(dht, test);
//     if (res) {
//       // printf("insert(%zu)\n", inserted);
//       ASSERT_TRUE(res);
//       added.push_back(res->id);
//       {
//         const dht::Node *fres = dht::find_contact(dht, res->id);
//         ASSERT_TRUE(fres);
//       }
//     }
//   }
//   // printf("added routing nodes %zu\n", count_nodes(dht.root));
//   // printf("added contacts: %zu\n", added.size());
//
//   for (auto &current : added) {
//     assert_present(dht, current);
//   }
//
//   assert_count(dht);
//   self_should_be_last(dht);
// }
