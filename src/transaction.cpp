#include "transaction.h"

#include <cstring>
#include <util/assert.h>

namespace tx {
//=====================================
using dht::Client;
using dht::Config;
using dht::DHT;

//=====================================
static Timestamp
expire_time(const Tx &tx) {
  Config config;
  return tx.sent + config.transaction_timeout;
}

static bool
is_sent(const Tx &tx) noexcept {
  return tx.sent != Timestamp(0);
}

static bool
is_expired(const Tx &tx, const Timestamp &now) noexcept {
  if (is_sent(tx)) {
    if (expire_time(tx) > now) {
      return false;
    }
    assertxs(expire_time(tx) <= now, uint64_t(expire_time(tx)), uint64_t(now));
  }

  return true;
}

//=====================================
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

static std::size_t
debug_count_free_transactions(const DHT &dht) noexcept {
  std::size_t result = 0;
  const Client &client = dht.client;
  const Tx *const head = client.timeout_head;
  const Tx *it2 = head;
  if (it2) {
    do {
      if (is_expired(*it2, dht.now)) {
        ++result;
      }
      it2 = it2->timeout_next;
    } while (it2 != head);
  }
  return result;
}

static void
debug_print_list(const DHT &dht) {
  const Client &client = dht.client;
  const Tx *const head = client.timeout_head;
  const Tx *it2 = head;
  do {
    printf("%p[sent:%lu]->", (void *)it2, std::uint64_t(it2->sent));
    it2 = it2->timeout_next;
  } while (it2 != head);
  printf("\n");
}

static bool
debug_is_ordered(const DHT &dht, Tx *const tout, bool is_exp,
                 bool is_snt) noexcept {
  const Client &client = dht.client;
  const Tx *const head = client.timeout_head;
  const Tx *it = head;

  const Tx *priv = nullptr;
Lit:
  if (it) {
    Timestamp priv_sent = priv ? priv->sent : Timestamp(0);
    /* $it should have been sent later or the same time as the previous. */
    if (!(it->sent >= priv_sent)) {
      debug_print_list(dht);

      assertxs(false, std::uint64_t(it->sent), std::uint64_t(priv_sent),
               (void *)it, (void *)tout, (void *)head, is_exp, is_snt);
      return false;
    }

    priv = it;
    it = it->timeout_next;

    assertx(it->timeout_priv == priv);

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
enlist(Client &client, Tx *const t) noexcept {
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
  enlist(self, t);
  self.timeout_head = t;

  assertxs((before + 1) == debug_count(self), (before + 1));
}

static void
deinit(Client &self) noexcept {
  // TODO cleanup active closures
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
consume_transaction(dht::DHT &dht, const krpc::Transaction &needle,
                    /*OUT*/ TxContext &out) noexcept {
  dht::Client &self = dht.client;

  assertx(debug_count(self) == Client::tree_capacity);

  constexpr std::size_t c = sizeof(Tx::prefix) + sizeof(Tx::suffix);
  if (needle.length == c) {
    Tx *const tx = search(self.tree, needle);

    if (tx) {
      // Note: compare prefix in tree, here compare both prefix and suffix
      if (tx->operator==(needle)) {
        assertx(self.active > 0);
        --self.active;

        out = tx->context;
        out.latency = dht.now - tx->sent;

        reset(*tx);
        assertx(!is_sent(*tx));
        move_front(self, tx);

        return true;
      } else {
        fprintf(stderr, "[]Suffix not found tx: %.*s\n", (int)needle.length,
                (char *)needle.id);
      }
    } else {
      fprintf(stderr, "[]Missing tx: %.*s\n", (int)needle.length,
              (char *)needle.id);
    }
  } else {
    fprintf(stderr, "[]Invalid length tx: %.*s (%zu)\n", (int)needle.length,
            (char *)needle.id, needle.length);
  }

  return false;
}

//=====================================
bool
has_free_transaction(const DHT &dht) {
  const Client &client = dht.client;
  const Tx *const head = client.timeout_head;
  if (head) {
    if (is_expired(*head, dht.now)) {
      return true;
    }
  } else {
    assertx(false);
  }
  assertx(debug_count_free_transactions(dht) == 0);
  return false;
}

//=====================================
#if 0
static Tx *
unlink_free(DHT &dht) noexcept {
  Client &client = dht.client;
  Tx *const head = client.timeout_head;
  Tx *result = nullptr;
  bool is_exp = false;
  bool is_snt = false;

  // auto cnt = debug_count(dht);
  // printf("cnt %zu\n", cnt);
  assertx(debug_is_ordered(dht, NULL, is_exp, is_snt));

  if (has_free_transaction(dht)) {
    is_exp = true;
    if (is_sent(*head)) {
      is_snt = true;
      assertx(client.active > 0);
      --client.active;

      head->context.timeout(dht, head);
      reset(*head);
    }

    assertx(debug_find(dht.client, head));
    result = unlink(client, head);
    assertx(!debug_find(dht.client, head));
  } // if is_expired

  assertx(debug_is_ordered(dht, head, is_exp, is_snt));

  return result;
}
#endif

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
  enlist(client, t);
  client.timeout_head = t->timeout_next;
}

bool
mint_transaction(DHT &dht, krpc::Transaction &out, TxContext &ctx) noexcept {
  bool is_exp = false;
  bool is_snt = false;
  assertx(debug_count(dht) == Client::tree_capacity);
  assertx(debug_is_ordered(dht, NULL, is_exp, is_snt));

  Client &self = dht.client;

  Tx *const tx = self.timeout_head;
  if (tx) {
    if (is_sent(*tx) && is_expired(*tx, dht.now)) {
      assertx(self.active > 0);
      --self.active;

      tx->context.timeout(dht, tx);
      reset(*tx);
      assertx(!is_sent(*tx));
    }

    if (!is_sent(*tx)) {
      ++self.active;
      make(*tx);

      out = krpc::Transaction(tx->prefix, tx->suffix);

      tx->context = ctx;
      assertx(dht.now > sp::Timestamp(0));
      tx->sent = dht.now;

      unlink(self, tx);
      add_back(self, tx);

      assertx(debug_is_ordered(dht, tx, is_exp, is_snt));

      return true;
    }
  }

  assertx(debug_count(dht) == Client::tree_capacity);
  assertx(debug_is_ordered(dht, NULL, is_exp, is_snt));

  return false;
} // dht::min_transaction()

//=====================================
bool
is_valid(DHT &self, const krpc::Transaction &needle) noexcept {
  assertx(debug_count(self) == Client::tree_capacity);
  assertx(debug_is_ordered(self, NULL, false, false));

  Client &client = self.client;
  Tx *const tx = search(client.tree, needle);
  if (tx) {
    if (!is_expired(*tx, self.now)) {
      return true;
    }
  }

  return false;
}

//=====================================
Timestamp
next_available(const dht::DHT &self) noexcept {
  assertx(debug_count(self) == Client::tree_capacity);
  assertx(debug_is_ordered(self, NULL, false, false));

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
eager_tx_timeout(dht::DHT &dht) noexcept {
  dht::Client &self = dht.client;

  assertx(debug_count(dht) == Client::tree_capacity);
  assertx(debug_is_ordered(dht, NULL, false, false));

  // [[sent=0], [sent=1, expired=true], [sent=1, expired=false]]
  Tx *const head = self.timeout_head;
  Tx *it = head;
  if (it) {
    do {
      Tx *const next = it->timeout_next;
      if (is_sent(*it) && is_expired(*it, dht.now)) {
        assertx(self.active > 0);
        --self.active;

        it->context.timeout(dht, it);
        reset(*it);
        assertx(!is_sent(*it));
        assertx(is_expired(*it, dht.now));

        assertx(debug_is_ordered(dht, it, true, true));
      } else if (is_sent(*it) && !is_expired(*it, dht.now)) {
        break;
      } else {
      }
      it = next;
    } while (it != head);
  }

  assertx(debug_count(dht) == Client::tree_capacity);
  assertx(debug_is_ordered(dht, NULL, false, false));
}

//=====================================
const Tx *
next_timeout(const dht::DHT &self) noexcept {
  const dht::Client &client = self.client;
  const Config &cfg = self.config;
  Tx *const head = client.timeout_head;
  Tx *it = head;
  if (it) {
    do {
      assertx(it->timeout_next);
      assertx(it->timeout_priv);
      if (is_sent(*it)) {
        assertxs(!is_expired(*it, self.now), uint64_t(it->sent),
                 uint64_t(it->sent + cfg.transaction_timeout),
                 uint64_t(self.now));
        return it;
      }

      it = it->timeout_next;
    } while (it != head);
  }

  return nullptr;
}

} // namespace tx
//=====================================
namespace dht {
/*dht::Client*/
Client::Client(fd &udp_fd, fd &_priv_fd) noexcept
    : udp(udp_fd)
    , priv_fd(_priv_fd)
    , timeout_head(nullptr)
    , buffer{}
    , tree{buffer}
    , active(0)
    , deinit(nullptr) {
  sp::byte a = 'a';
  sp::byte b = 'a';
  std::size_t cnt = 0;
  in_order_for_each(this->tree, [this, &a, &b, &cnt](tx::Tx &tx) {
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
    add_front(*this, &tx);
  });

  assertx(tx::debug_count(*this) == Client::tree_capacity);
  assertx(Client::tree_capacity == cnt);

  this->deinit = tx::deinit;
}

Client::~Client() noexcept {
  if (this->deinit) {
    this->deinit(*this);
  }
}

} // namespace dht
