#include "transaction.h"

#include <cstring>
#include <util/assert.h>

namespace tx {

using dht::Client;
using dht::Config;
using dht::DHT;

static void
add_front(Client &, Tx *) noexcept;

static void
reset(Tx &tx) noexcept {
  reset(tx.context);
  tx.sent = Timestamp(0);

  tx.suffix[0] = '\0';
  tx.suffix[1] = '\0';
}

static std::size_t
debug_count(Client &client) noexcept {
  Tx *const head = client.timeout_head;
  Tx *it = head;
  std::size_t result = 0;
Lit:
  if (it) {
    ++result;

    assertx(it == it->timeout_next->timeout_priv);
    it = it->timeout_next;

    if (it != head) {
      goto Lit;
    }
  } else {
    assertx(false);
  }

  return result;
}

static std::size_t
debug_count(DHT &dht) noexcept {
  Client &client = dht.client;
  return debug_count(client);
}

static bool
is_ordered(DHT &dht) noexcept {
  Client &client = dht.client;
  Tx *const head = client.timeout_head;
  Tx *it = head;

  Timestamp t = it ? it->sent : Timestamp(0);
Lit:
  if (it) {
    /* Current $it should have been sent later or at the same time as the
     * previous.
     */
    if (!(it->sent >= t)) {
      return false;
    }

    t = it->sent;
    it = it->timeout_next;

    if (it != head) {
      goto Lit;
    }
  } else {
    assertx(false);
  }

  return true;
}

bool
init(Client &client) noexcept {
  sp::byte a = 'a';
  sp::byte b = 'a';
  in_order_for_each(client.tree, [&client, &a, &b](Tx &tx) {
    assertx(tx.prefix[0] == '\0');
    assertx(tx.prefix[1] == '\0');

    tx.prefix[0] = a;
    tx.prefix[1] = b++;
    if (b == sp::byte('z')) {
      ++a;
      b = 'a';
    }
    // printf("prefix: %c%c\n", tx.prefix[0], tx.prefix[1]);
    add_front(client, &tx);
  });
  assertx(debug_count(client) == Client::tree_capacity);
  return true;
}

static Tx *
search(binary::StaticTree<Tx> &tree, const krpc::Transaction &needle) noexcept {
  Tx *const result = (Tx *)find(tree, needle);
  if (!result) {
    // assert
    in_order_for_each(tree, [&needle](auto &current) {
      if (current == needle) {
        assertx(false);
      }
    });
  }
  return result;
}

static Tx *
unlink(Client &client, Tx *t) noexcept {
  if (client.timeout_head == t) {
    auto *next = t->timeout_next != t ? t->timeout_next : nullptr;
    client.timeout_head = next;
  }

  auto *next = t->timeout_next;
  auto *priv = t->timeout_priv;

  next->timeout_priv = priv;
  priv->timeout_next = next;

  t->timeout_next = nullptr;
  t->timeout_priv = nullptr;

  return t;
}

static bool
is_sent(const Tx &tx) noexcept {
  return tx.sent != Timestamp(0);
}

static Timestamp
expire_time(const Tx &tx) {
  Config config;
  return tx.sent + config.transaction_timeout;
}

static bool
is_expired(const Tx &tx, Timestamp now) noexcept {
  if (is_sent(tx)) {
    if (expire_time(tx) > now) {
      return false;
    }
  }
  return true;
}

static void
ssss(Client &client, Tx *const t) noexcept {
  assertx(t->timeout_next == nullptr);
  assertx(t->timeout_priv == nullptr);

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
consume(Client &client, const krpc::Transaction &needle,
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
} // dht::consume()

static Tx *
unlink_free(DHT &dht, Timestamp now) noexcept {
  Client &client = dht.client;
  Tx *const head = client.timeout_head;

  // auto cnt = debug_count(dht);
  // printf("cnt %zu\n", cnt);
  assertx(debug_count(dht) == Client::tree_capacity);
  assertx(is_ordered(dht));

  if (head) {
    if (is_expired(*head, now)) {
      if (is_sent(*head)) {
        --client.active;
      }

      head->context.cancel(dht);
      reset(*head);

      return unlink(client, head);
    } // if is_expired

  } else {
    assertx(false);
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
mint(DHT &dht, krpc::Transaction &out, TxContext &ctx) noexcept {
  Client &client = dht.client;

  Tx *const tx = unlink_free(dht, /*timeout*/ dht.now);
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

Timestamp
next_available(const dht::DHT &self) noexcept {
  const Client &client = self.client;
  const Tx *const head = client.timeout_head;
  const Config &cfg = self.config;

  if (!head) {
    // some arbitrary date in the future
    return self.now + cfg.refresh_interval;
  }

  if (is_expired(*head, self.now)) {
    return self.now;
  }

  return expire_time(*head);
}

} // namespace tx
