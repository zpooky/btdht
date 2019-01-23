#include "transaction.h"

#include <cstring>
#include <util/assert.h>

namespace tx {
//=====================================
using dht::Client;
using dht::Config;
using dht::DHT;

static bool
debug_find(const Client &client, Tx *const needle) noexcept {
  assertx(needle);
  const Tx *const head = client.timeout_head;
  const Tx *it = head;

Lit:
  if (it) {
    if (it == needle) {
      return true;
    }

    assertx(it == it->timeout_next->timeout_priv);
    it = it->timeout_next;

    if (it != head) {
      goto Lit;
    }
  }

  return false;
}

static std::size_t
debug_count(const Client &client) noexcept {
  const Tx *const head = client.timeout_head;
  const Tx *it = head;
  std::size_t result = 0;

Lit:
  if (it) {
    ++result;

    assertx(it == it->timeout_next->timeout_priv);
    it = it->timeout_next;

    if (it != head) {
      goto Lit;
    }
  }

  return result;
}

static std::size_t
debug_count(const DHT &dht) noexcept {
  return debug_count(dht.client);
}

static bool
debug_is_ordered(DHT &dht) noexcept {
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

//=====================================
static void
ssss(Client &client, Tx *const t) noexcept {
  assertx(t);
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
add_front(Client &self, Tx *t) noexcept {
  assertx(t);

  assertx(!debug_find(self, t));

  std::size_t before = debug_count(self); // TODO only in debug
  // TODO assert $i is not present in queue
  ssss(self, t);
  self.timeout_head = t;

  assertxs((before + 1) == debug_count(self), (before + 1));
}

bool
init(Client &self) noexcept {
  sp::byte a = 'a';
  sp::byte b = 'a';
  std::size_t cnt = 0;
  in_order_for_each(self.tree, [&self, &a, &b, &cnt](Tx &tx) {
    assertx(tx.prefix[0] == '\0');
    assertx(tx.prefix[1] == '\0');

    ++cnt;

    tx.prefix[0] = a;
    tx.prefix[1] = b++;
    if (b == sp::byte('z')) {
      ++a;
      b = 'a';
    }
    // printf("prefix: %c%c\n", tx.prefix[0], tx.prefix[1]);
    add_front(self, &tx);
  });

  assertx(debug_count(self) == Client::tree_capacity);
  assertx(Client::tree_capacity == cnt);

  return true;
}

//=====================================
static Tx *
unlink(Client &self, Tx *t) noexcept {
  assertx(self.timeout_head);
  assertx(t);

  if (self.timeout_head == t) {
    auto *next = t->timeout_next != t ? t->timeout_next : nullptr;
    self.timeout_head = next;
  }

  auto *next = t->timeout_next;
  auto *priv = t->timeout_priv;

  next->timeout_priv = priv;
  priv->timeout_next = next;

  t->timeout_next = nullptr;
  t->timeout_priv = nullptr;

  assertx(debug_count(self) == (Client::tree_capacity - 1));

  return t;
}

static bool
is_sent(const Tx &tx) noexcept {
  return tx.sent != Timestamp(0);
}

static Tx *
search(binary::StaticTree<Tx> &tree, const krpc::Transaction &needle) noexcept {
  Tx *const result = (Tx *)find(tree, needle);
  if (!result) {
    // TODO only assert
    in_order_for_each(tree, [&needle](auto &current) {
      if (current == needle) {
        assertx(false);
      }
    });
  }
  return result;
}

static void
reset(Tx &tx) noexcept {
  reset(tx.context);
  tx.sent = Timestamp(0);

  tx.suffix[0] = '\0';
  tx.suffix[1] = '\0';
}

static void
move_front(Client &self, Tx *tx) noexcept {
  assertx(tx);
  assertx(debug_find(self, tx));

  unlink(self, tx);

  assertx(!debug_find(self, tx));

  add_front(self, tx);

  assertx(debug_find(self, tx));
}

bool
consume(Client &self, const krpc::Transaction &needle,
        /*OUT*/ TxContext &out) noexcept {

  assertx(debug_count(self) == Client::tree_capacity);

  constexpr std::size_t c = sizeof(Tx::prefix) + sizeof(Tx::suffix);
  if (needle.length == c) {

    Tx *const tx = search(self.tree, needle);
    if (tx) {

      if (*tx == needle) {
        out = tx->context;

        reset(*tx);
        move_front(self, tx);
        assertx(self.active > 0);
        --self.active;

        return true;
      }
    }
  }

  return false;
} // dht::consume()

//=====================================
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

static Tx *
unlink_free(DHT &dht, Timestamp now) noexcept {
  Client &client = dht.client;
  Tx *const head = client.timeout_head;
  Tx *result = nullptr;

  // auto cnt = debug_count(dht);
  // printf("cnt %zu\n", cnt);
  assertx(debug_is_ordered(dht));

  if (head) {
    if (is_expired(*head, now)) {
      if (is_sent(*head)) {
        assertx(client.active > 0);
        --client.active;

        head->context.cancel(dht, head);
        reset(*head);
      }

      assertx(debug_find(dht.client, head));
      result = unlink(client, head);
      assertx(!debug_find(dht.client, head));
    } // if is_expired

  } else {
    assertx(false);
  }

  return result;
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

static void
add_back(Client &client, Tx *t) noexcept {
  ssss(client, t);
  client.timeout_head = t->timeout_next;
}

bool
mint(DHT &dht, krpc::Transaction &out, TxContext &ctx) noexcept {
  assertx(debug_count(dht) == Client::tree_capacity);

  Client &client = dht.client;

  Tx *const tx = unlink_free(dht, dht.now);
  if (tx) {
    ++client.active;
    make(*tx);

    out = krpc::Transaction(tx->prefix, tx->suffix);

    tx->context = ctx;
    tx->sent = dht.now;
    add_back(client, tx);

    return true;
  }

  return false;
} // dht::min_transaction()

//=====================================
bool
is_valid(DHT &dht, const krpc::Transaction &needle) noexcept {
  assertx(debug_count(dht) == Client::tree_capacity);

  Client &client = dht.client;
  Tx *const tx = search(client.tree, needle);
  if (tx) {
    if (!is_expired(*tx, dht.now)) {
      return true;
    }
  }

  return false;
}

//=====================================
Timestamp
next_available(const dht::DHT &self) noexcept {
  assertx(debug_count(self) == Client::tree_capacity);

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

//=====================================
void
eager_tx_timeout(dht::DHT &self, sp::Milliseconds timeout) noexcept {
  assertx(debug_count(self) == Client::tree_capacity);

  dht::Client &client = self.client;
  Config config;

Lit:
  Tx *const head = client.timeout_head;
  if (head) {
    if (is_sent(*head)) {
      auto max = head->sent + timeout;
      if (self.now > max) {
        assertx(client.active > 0);
        --client.active;

        head->context.cancel(self, head);
        reset(*head);

        unlink(client, head);
        add_back(client, head);
        goto Lit;
      }
    }
  }
}

//=====================================

const Tx *
next_timeout(const dht::Client &client) noexcept {
  Tx *const head = client.timeout_head;
  if (head) {
    if (!is_sent(*head)) {
      return nullptr;
    }
  }

  return head;
}

} // namespace tx
