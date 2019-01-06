#include "timeout.h"
#include "util.h"
#include "gtest/gtest.h"
#include <dht.h>
#include <hash/fnv.h>
#include <list>
#include <map/HashSetOpen.h>
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

  ASSERT_EQ(contact.ping_outstanding, std::uint8_t(0));
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
      dht.id.id[19] = ~dht.id.id[19];
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
    dht.id.id[19] = ~dht.id.id[19];                                            \
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
  dht::DHT dht(sock, c, r);
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
  dht::DHT dht(sock, c, r);
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
  dht::DHT dht(sock, c, r);
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
  for (std::size_t i = 0; i < 10000; ++i) {
    Ip ip(i);
    Contact c(ip, 0);
    dht::DHT dht(sock, c, r);
    init(dht);
    ASSERT_TRUE(is_strict(ip, dht.id));
  }
}

TEST(dhtTest, test_node_id_not_strict) {
  fd sock(-1);
  prng::xorshift32 r(1);
  dht::NodeId id;
  for (std::size_t i = 0; i < 500000; ++i) {
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
  dht::DHT dht(sock, c, r);
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
  dht::DHT dht(sock, c, r);
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

  static std::size_t
  rank(const NodeId &id, const Key &o) noexcept {
    std::size_t i = 0;
    for (; i < NodeId::bits; ++i) {
      if (bit(id.id, i) != bit(o, i)) {
        return i;
      }
    }

    return i;
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
  std::uint64_t
  operator()(const RankNodeId &n) const noexcept {
    return operator()(n.id);
  }
};
} // namespace sp

TEST(dhtTest, test_self_rand) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  dht::DHT dht(sock, c, r);
  dht::init(dht);
  heap::StaticMaxBinary<RankNodeId, 1024> heap;
  sp::HashSetOpen<NodeId> set;

  for (std::size_t i = 0; i < 1024; ++i) {

    for (std::size_t x = 0; x < capacity(heap); ++x) {
      auto strt = uniform_dist(dht.random, 0, NodeId::bits);

      Node node;
      node.id = dht.id;
      for (std::size_t a = strt; a < NodeId::bits; ++a) {
        node.id.set_bit(a, uniform_bool(dht.random));
      }
      printf("%s\n", to_hex(node.id));

      insert(dht, node);
      if (!lookup(set, node.id)) {
        insert(set, node.id);
        insert_eager(heap, RankNodeId(dht, node.id));
      }
    }
  }
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
