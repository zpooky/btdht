#include "util.h"
#include <transaction.h>

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
shuffle_tx(krpc::Transaction (&arr)[SIZE]) {
  // TODO
}

TEST(transactionTest, test_valid_tree) {
  fd s(-1);
  dht::DHT dht(s, ExternalIp(0, 0));
  dht.now = time(nullptr);
  assert(dht::init(dht.client));
  // TODO
}

TEST(transactionTest, test) {
  fd s(-1);
  dht::DHT dht(s, ExternalIp(0, 0));
  dht.now = time(nullptr);
  assert(dht::init(dht.client));

  constexpr std::size_t IT = dht::TxTree::capacity;
  krpc::Transaction ts[IT] = {};

  printf("nodes: %zu\n", IT);
  for (std::size_t i = 0; i < IT; ++i) {
    for (std::size_t k = 0; k < i; ++k) {
      printf("k: %zu\n", k);
      ASSERT_TRUE(dht::is_valid(dht, ts[k]));
    }
    dht::TxContext h;
    ASSERT_TRUE(dht::mint_tx(dht, ts[i], h));
    // printf("%c%c\n", ts[i].id[0], ts[i].id[1]);
  }
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

    krpc::Transaction tx;
    dht::TxContext h;
    ASSERT_TRUE(dht::take_tx(dht.client, tx, h));
  }
}
