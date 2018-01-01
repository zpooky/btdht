#include "transaction.h"

#include <cassert>
#include <cstring>

namespace dht {

template <typename F>
static void
in_order(TxTree &, F) noexcept;

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
  in_order(client.tree, [&client, &a, &b](Tx &tx) {
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

static std::size_t
level(std::size_t l) noexcept {
  if (l == 0) {
    return 0;
  }
  return std::size_t(std::size_t(1) << l) - std::size_t(1);
}

static std::size_t
translate(std::size_t l, std::size_t idx) noexcept {
  std::size_t start = level(l);
  return std::size_t(idx + start);
}

enum class Direction : std::uint8_t { LEFT, RIGHT };

static std::size_t
lookup_relative(std::size_t old_idx, Direction dir) noexcept {

  constexpr std::size_t children = 2;
  std::size_t idx = old_idx * children;
  if (dir == Direction::RIGHT) {
    idx = idx + 1;
  }

  return idx;
}

static std::size_t
parent_relative(std::size_t idx) noexcept {
  std::size_t i(idx / 2);

  if (lookup_relative(i, Direction::RIGHT) == idx) {
    return i;
  }

  if (lookup_relative(i, Direction::LEFT) == idx) {
    return i;
  }

  assert(false);
  return 0;
}

static Tx *
search(TxTree &tree, const krpc::Transaction &needle) noexcept {
  std::size_t level(0);
  std::size_t idx(0);

Lstart:
  const std::size_t abs_idx = translate(level, idx);

  if (abs_idx < TxTree::capacity) {
    Tx &current = tree[abs_idx];
    int c = current.cmp(needle);
    if (c == 0) {
      return &current;
    }

    level++;
    Direction dir = c == -1 ? Direction::LEFT : Direction::RIGHT;
    idx = lookup_relative(idx, dir);

    goto Lstart;
  }
  return nullptr;
}

template <typename F>
static void
in_order(TxTree &tree, F f) noexcept {
  enum class Traversal : uint8_t { UP, DOWN };
  // printf("tree:%zu\n", TxTree::capacity);

  Direction d[TxTree::levels + 1]{Direction::LEFT};
  Traversal t = Traversal::UP;

  auto set_direction = [&d](std::size_t lvl, Direction dir) {
    if (lvl <= TxTree::levels) {
      d[lvl] = dir;
    }
  };

  std::size_t level = 0;
  std::size_t idx(0);
Lstart:
  if (level <= TxTree::levels) {
    if (t == Traversal::UP) {
      if (d[level] == Direction::LEFT) {
        level++;
        set_direction(level, Direction::LEFT);
        idx = lookup_relative(idx, Direction::LEFT);
      } else {
        level++;
        set_direction(level, Direction::LEFT);
        idx = lookup_relative(idx, Direction::RIGHT);
      }
      goto Lstart;
    } else /*t == DOWN*/ {
      if (d[level] == Direction::LEFT) {
        // We returned to this level after traversed left, now traverse right

        // Indicate that we have consumed right when returning to this level[0]
        d[level] = Direction::RIGHT;

        const std::size_t abs_idx = translate(level, idx);
        f(tree[abs_idx]);

        t = Traversal::UP;
        ++level;
        set_direction(level, Direction::LEFT);
        idx = lookup_relative(idx, Direction::RIGHT);

        goto Lstart;
      } else {
        if (level > 0) {
          idx = parent_relative(idx);
          level--;
          goto Lstart;
        }
      }
    }
  } else {
    assert(t == Traversal::UP);
    // level and index now point to an out of bound node
    idx = parent_relative(idx);
    level--;

    t = Traversal::DOWN;
    goto Lstart;
  }
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
      head->context.cancel(dht);
      reset(*head);

      return unlink(client, head);
    }
  }

  return nullptr;
}

bool
mint_tx(DHT &dht, krpc::Transaction &out, TxContext &ctx) noexcept {
  Client &client = dht.client;

  Tx *const tx = unlink_free(dht, dht.now);
  if (tx) {
    {
      const int r = rand();
      static_assert(sizeof(tx->suffix) <= sizeof(r), "");
      std::memcpy(tx->suffix, &r, sizeof(tx->suffix));

      for (std::size_t i = 0; i < sizeof(tx->suffix); ++i) {
        if (tx->suffix[i] == 0) {
          tx->suffix[i]++;
        }
      }
    }

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
