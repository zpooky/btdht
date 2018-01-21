#include "transaction.h"

#include <cassert>
#include <cstring>

namespace dht {

static void
add_front(Client &, Tx *) noexcept;

static void
reset(Tx &tx) noexcept {
  reset(tx.context);
  tx.sent = 0;

  tx.suffix[0] = '\0';
  tx.suffix[1] = '\0';
}

bool
init(Client &client) noexcept {
  sp::byte a = 'a';
  sp::byte b = 'a';
  bst::in_order_for_each(client.tree, [&client, &a, &b](Tx &tx) {
    assert(tx.prefix[0] == '\0');
    assert(tx.prefix[1] == '\0');

    tx.prefix[0] = a;
    tx.prefix[1] = b++;
    if (b == sp::byte('z')) {
      ++a;
      b = 'a';
    }
    // printf("prefix: %c%c\n", tx.prefix[0], tx.prefix[1]);
    add_front(client, &tx);
  });
  return true;
}

static Tx *
search(bst::StaticTree<Tx> &tree, const krpc::Transaction &needle) noexcept {
  return (Tx *)bst::find(tree, needle);
}

static Tx *
unlink(Client &client, Tx *t) noexcept {
  if (client.timeout_head == t) {
    auto *next = t->timeout_next != t ? t->timeout_next : nullptr;
    client.timeout_head = next;
  }

  auto *next = client.timeout_head;
  auto *priv = next->timeout_priv;

  next->timeout_priv = priv;
  priv->timeout_next = next;

  t->timeout_next = nullptr;
  t->timeout_priv = nullptr;

  return t;
}

static bool
is_sent(const Tx &tx) noexcept {
  return tx.sent != 0;
}

static bool
is_expired(const Tx &tx, time_t now) noexcept {
  if (is_sent(tx)) {
    Config config;
    if ((tx.sent + config.transaction_timeout) > now) {
      return false;
    }
  }
  return true;
}
static void
ssss(Client &client, Tx *const t) noexcept {
  assert(t->timeout_next == nullptr);
  assert(t->timeout_priv == nullptr);

  if (client.timeout_head) {
    auto *next = client.timeout_head;
    auto *priv = next->timeout_priv;

    next->timeout_priv = t;
    priv->timeout_next = t;

    t->timeout_next = next;
    t->timeout_priv = priv;
  } else {
    t->timeout_next = t;
    t->timeout_priv = t;
  }
}

static void
add_back(Client &client, Tx *t) noexcept {
  ssss(client, t);
  client.timeout_head = t->timeout_next;
}

static void
add_front(Client &client, Tx *t) noexcept {
  ssss(client, t);
  client.timeout_head = t;
}

static void
move_front(Client &client, Tx *tx) noexcept {
  unlink(client, tx);
  add_front(client, tx);
}

bool
take_tx(Client &client, const krpc::Transaction &needle,
        /*OUT*/ TxContext &out) noexcept {
  constexpr std::size_t c = sizeof(Tx::prefix) + sizeof(Tx::suffix);
  if (needle.length == c) {

    Tx *const tx = search(client.tree, needle);
    if (tx) {

      if (*tx == needle) {
        out = tx->context;
        reset(*tx);
        move_front(client, tx);
        --client.active;

        return true;
      }
    }
  }

  return false;
} // dht::take_tx()

static Tx *
unlink_free(DHT &dht, time_t now) noexcept {
  Client &client = dht.client;
  Tx *const head = client.timeout_head;

  if (head) {

    if (is_expired(*head, now)) {
      if (is_sent(*head)) {
        --client.active;
      }
      head->context.cancel(dht);
      reset(*head);

      return unlink(client, head);
    }
  }

  return nullptr;
}
static void
make(Tx &tx) noexcept {
  const int r = rand();
  static_assert(sizeof(tx.suffix) <= sizeof(r), "");

  std::memcpy(tx.suffix, &r, sizeof(tx.suffix));

  for (std::size_t i = 0; i < sizeof(tx.suffix); ++i) {
    // Do not use 0 in tx
    if (tx.suffix[i] == 0) {
      tx.suffix[i]++;
    }
  }
}

bool
mint_tx(DHT &dht, krpc::Transaction &out, TxContext &ctx) noexcept {
  Client &client = dht.client;

  Tx *const tx = unlink_free(dht, dht.now);
  if (tx) {
    ++client.active;
    make(*tx);

    std::memcpy(out.id, tx->prefix, sizeof(tx->prefix));
    out.length = sizeof(tx->prefix);
    std::memcpy(out.id + out.length, tx->suffix, sizeof(tx->suffix));
    out.length += sizeof(tx->suffix);

    tx->context = ctx;
    tx->sent = dht.now;
    add_back(client, tx);

    return true;
  }

  return false;
} // dht::min_transaction()

bool
is_valid(DHT &dht, const krpc::Transaction &needle) noexcept {
  Client &client = dht.client;
  Tx *const tx = search(client.tree, needle);
  if (tx) {
    if (!is_expired(*tx, dht.now)) {
      return true;
    }
  }

  return false;
}

} // namespace dht
