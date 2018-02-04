#include "util.h"
#include <transaction.h>

using namespace dht;

template <std::size_t SIZE>
static void
test_unique(krpc::Transaction (&arr)[SIZE]) {
  for (std::size_t i = 0; i < SIZE; ++i) {
    for (std::size_t k = i + 1; k < SIZE; ++k) {
      // printf("i:%zu, k:%zu\n", i, k);
      // printf("i: %c%c\n", arr[i].id[0], arr[i].id[1]);
      // printf("k: %c%c\n", arr[k].id[0], arr[k].id[1]);
      ASSERT_FALSE(arr[i].id[0] == arr[k].id[0] &&
                   arr[i].id[1] == arr[k].id[1]);
    }
  }
}

template <std::size_t SIZE>
static void
shuffle_tx(krpc::Transaction (&)[SIZE]) {
  // TODO
}

TEST(transactionTest, test_valid_tree) {
  fd s(-1);
  dht::DHT dht(s, Contact(0, 0));
  dht.now = time(nullptr);
  assert(dht::init(dht.client));
  // TODO
}

TEST(transactionTest, test_valid) {
  std::size_t test_it = 0;
  fd s(-1);
  dht::DHT dht(s, Contact(0, 0));
  dht.now = time(nullptr);
  ASSERT_TRUE(dht::init(dht.client));

Lrestart:
  if (test_it++ < 100) {
    constexpr std::size_t IT = Client::tree_capacity;
    krpc::Transaction ts[IT] = {};

    // printf("nodes: %zu\n", IT);
    for (std::size_t i = 0; i < IT; ++i) {
      for (std::size_t k = 0; k < i; ++k) {
        // printf("k: %zu\n", k);
        ASSERT_TRUE(dht::is_valid(dht, ts[k]));
      }
      dht::TxContext h;
      ASSERT_TRUE(dht::mint_tx(dht, ts[i], h));
      // printf("Mint_Tx: %c%c\n", ts[i].id[0], ts[i].id[1]);
      ASSERT_TRUE(dht::is_valid(dht, ts[i]));
    } // for

    {
      krpc::Transaction tx;
      dht::TxContext h;
      ASSERT_FALSE(dht::mint_tx(dht, tx, h));
    }
    test_unique(ts);

    shuffle_tx(ts);
    for (std::size_t i = 0; i < IT; ++i) {
      for (std::size_t k = 0; k < i; ++k) {
        ASSERT_FALSE(dht::is_valid(dht, ts[k]));
      }
      for (std::size_t k = i; k < IT; ++k) {
        ASSERT_TRUE(dht::is_valid(dht, ts[k]));
      }

      dht::TxContext h;
      // printf("i: %zu\n", i);
      ASSERT_TRUE(dht::is_valid(dht, ts[i]));
      ASSERT_TRUE(dht::take_tx(dht.client, ts[i], h));
      ASSERT_FALSE(dht::is_valid(dht, ts[i]));
    }
    goto Lrestart;
  }
}

TEST(transactionTest, asd) {
  sp::byte a = 'a';
  sp::byte b = 'a';
  auto asd = [&a, &b](Tx &tx) {
    assert(tx.prefix[0] == '\0');
    assert(tx.prefix[1] == '\0');

    tx.prefix[0] = a;
    tx.prefix[1] = b++;
    if (b == sp::byte('z')) {
      ++a;
      b = 'a';
    }
  };
  Tx buffer[1024];
  binary::StaticTree<Tx> tree(buffer);
  in_order_for_each(tree, [&asd](Tx &tx) {
    asd(tx);
    // printf("prefix: %c%c\n", tx.prefix[0], tx.prefix[1]);
  });

  for (std::size_t i = 0; i < 255; ++i) {
    // printf("%");
  }

  a = 'a';
  b = 'a';

  for (std::size_t i = 0; i < tree.capacity; ++i) {
    Tx tx;
    asd(tx);
    const Tx *res = find(tree, tx);
    // if (res) {
    // printf("prefix: %c%c\n", tx.prefix[0], tx.prefix[1]);
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->prefix[0], tx.prefix[0]);
    ASSERT_EQ(res->prefix[1], tx.prefix[1]);

    ASSERT_EQ(res->suffix[0], tx.suffix[0]);
    ASSERT_EQ(res->suffix[1], tx.suffix[1]);
    // }
  }
}
