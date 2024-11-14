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
shuffle_tx(prng::xorshift32 &r, krpc::Transaction (&vec)[SIZE]) {
  shuffle_array(r, vec, SIZE);
}

TEST(transactionTest, test_mint_consume) {
  fd s(-1);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::Client client{s, s};
  dht::Options opt;
  dht::DHT dht(Contact(0, 0), client, r, now, opt);
  fprintf(stderr, "%s:sizeof(%zuKB)\n", __func__, sizeof(dht::DHT) / 1024);

  krpc::Transaction ts[Client::tree_capacity] = {};

  for (size_t i = 0; i < Client::tree_capacity; ++i) {
    tx::TxContext h;
    ASSERT_TRUE(tx::has_free_transaction(dht));
    ASSERT_TRUE(tx::mint_transaction(dht, ts[i], h));
  }
  ASSERT_FALSE(tx::has_free_transaction(dht));
  {
    krpc::Transaction dummy;
    tx::TxContext h;
    ASSERT_FALSE(tx::mint_transaction(dht, dummy, h));
  }
  shuffle_tx(r, ts);
  for (size_t i = 0; i < Client::tree_capacity; ++i) {
    tx::TxContext h;
    ASSERT_TRUE(tx::consume_transaction(dht, ts[i], h));
    ASSERT_FALSE(tx::consume_transaction(dht, ts[i], h));
    ASSERT_TRUE(tx::has_free_transaction(dht));
  }
}

static size_t global_count = 0;

TEST(transactionTest, test_mint_timeout) {
  fd s(-1);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::Client client{s, s};
  dht::Options opt;
  dht::DHT dht(Contact(0, 0), client, r, now, opt);
  fprintf(stderr, "%s:sizeof(%zuKB)\n", __func__, sizeof(dht::DHT) / 1024);

  krpc::Transaction ts[Client::tree_capacity] = {};
  tx::TxContext h;
  global_count = 0;
  h.int_timeout = [](dht::DHT &, const krpc::Transaction &, const Timestamp &,
                     void *) { //
    global_count++;
  };
  for (size_t i = 0; i < Client::tree_capacity; ++i) {
    ASSERT_TRUE(tx::has_free_transaction(dht));
    ASSERT_TRUE(tx::mint_transaction(dht, ts[i], h));
  }
  ASSERT_EQ(0, global_count);
  {
    krpc::Transaction dummy;
    ASSERT_FALSE(tx::has_free_transaction(dht));
    ASSERT_FALSE(tx::mint_transaction(dht, dummy, h));
  }
  ASSERT_EQ(0, global_count);

  Config config;
  now = now + config.transaction_timeout;
  size_t i = 0;
  for (; i < Client::tree_capacity; ++i) {
    ASSERT_TRUE(tx::has_free_transaction(dht));
    ASSERT_EQ(i, global_count);
    ASSERT_TRUE(tx::mint_transaction(dht, ts[i], h));
  }
  ASSERT_EQ(i, global_count);
  {
    krpc::Transaction dummy;
    ASSERT_FALSE(tx::has_free_transaction(dht));
    ASSERT_FALSE(tx::mint_transaction(dht, dummy, h));
  }
  ASSERT_EQ(i, global_count);
}

TEST(transactionTest, test_mint_timeout2) {
  fd s(-1);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  Timestamp before = now;
  dht::Client client{s, s};
  dht::Options opt;
  dht::DHT dht(Contact(0, 0), client, r, now, opt);
  fprintf(stderr, "%s:sizeof(%zuKB)\n", __func__, sizeof(dht::DHT) / 1024);

  global_count = 0;
  tx::TxContext h;
  h.int_timeout = [](dht::DHT &, const krpc::Transaction &, const Timestamp &,
                     void *) { //
    global_count++;
  };
  for (size_t i = 0; i < Client::tree_capacity; ++i) {
    krpc::Transaction dummy;
    ASSERT_TRUE(tx::has_free_transaction(dht));
    ASSERT_TRUE(tx::mint_transaction(dht, dummy, h));
    dht.now = dht.now + sp::Seconds(1);
  }
  ASSERT_EQ(0, global_count);
  {
    krpc::Transaction dummy;
    ASSERT_FALSE(tx::has_free_transaction(dht));
    ASSERT_FALSE(tx::mint_transaction(dht, dummy, h));
  }

  Config config;
  size_t i = 0;

  for (; i < Client::tree_capacity; ++i) {
    ASSERT_EQ(i, global_count);

    ASSERT_FALSE(tx::has_free_transaction(dht));
    dht.now = before + config.transaction_timeout + sp::Seconds(i);
    ASSERT_TRUE(tx::has_free_transaction(dht));
    {
      krpc::Transaction dummy;
      ASSERT_TRUE(tx::mint_transaction(dht, dummy, h));
    }
    ASSERT_FALSE(tx::has_free_transaction(dht));
    {
      krpc::Transaction dummy;
      ASSERT_FALSE(tx::mint_transaction(dht, dummy, h));
    }
  } // for

  ASSERT_EQ(i, global_count);
}

TEST(transactionTest, test_eager_tx_timeout) {
  fd s(-1);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  Timestamp before = now;
  dht::Client client{s, s};
  dht::Options opt;
  dht::DHT dht(Contact(0, 0), client, r, now, opt);
  fprintf(stderr, "%s:sizeof(%zuKB)\n", __func__, sizeof(dht::DHT) / 1024);

  global_count = 0;
  tx::TxContext h;
  h.int_timeout = [](dht::DHT &, const krpc::Transaction &, const Timestamp &,
                     void *) { //
    global_count++;
  };
  for (size_t i = 0; i < Client::tree_capacity; ++i) {
    krpc::Transaction dummy;
    ASSERT_TRUE(tx::has_free_transaction(dht));
    ASSERT_TRUE(tx::mint_transaction(dht, dummy, h));
    dht.now = dht.now + sp::Seconds(1);
  }
  ASSERT_EQ(0, global_count);
  {
    krpc::Transaction dummy;
    ASSERT_FALSE(tx::has_free_transaction(dht));
    ASSERT_FALSE(tx::mint_transaction(dht, dummy, h));
  }
  ASSERT_FALSE(tx::has_free_transaction(dht));
  tx::eager_tx_timeout(dht);
  ASSERT_FALSE(tx::has_free_transaction(dht));
}

TEST(transactionTest, test_valid) {
  std::size_t test_it = 0;
  fd s(-1);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::Client client{s, s};
  dht::Options opt;
  dht::DHT dht(Contact(0, 0), client, r, now, opt);

Lrestart:
  if (test_it++ < 100) {
    constexpr std::size_t IT = Client::tree_capacity;
    krpc::Transaction ts[IT] = {};

    // printf("nodes: %zu\n", IT);
    for (std::size_t i = 0; i < IT; ++i) {
      for (std::size_t k = 0; k < i; ++k) {
        // printf("k: %zu\n", k);
        ASSERT_TRUE(tx::is_valid(dht, ts[k]));
      }
      tx::TxContext h;
      ASSERT_TRUE(tx::mint_transaction(dht, ts[i], h));
      // printf("mint_transaction: %c%c\n", ts[i].id[0], ts[i].id[1]);
      ASSERT_TRUE(tx::is_valid(dht, ts[i]));
    } // for

    {
      krpc::Transaction tx;
      tx::TxContext h;
      ASSERT_FALSE(tx::mint_transaction(dht, tx, h));
    }
    test_unique(ts);

    shuffle_tx(r, ts);
    for (std::size_t i = 0; i < IT; ++i) {
      for (std::size_t k = 0; k < i; ++k) {
        ASSERT_FALSE(tx::is_valid(dht, ts[k]));
      }
      for (std::size_t k = i; k < IT; ++k) {
        ASSERT_TRUE(tx::is_valid(dht, ts[k]));
      }

      tx::TxContext h;
      // printf("i: %zu\n", i);
      ASSERT_TRUE(tx::is_valid(dht, ts[i]));
      ASSERT_TRUE(tx::consume_transaction(dht, ts[i], h));
      ASSERT_FALSE(tx::is_valid(dht, ts[i]));
    }
    goto Lrestart;
  }
}

TEST(transactionTest, asd) {
  using tx::Tx;
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
