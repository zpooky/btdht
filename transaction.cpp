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
  tx.handle = nullptr;
  tx.sent = 0;

  tx.suffix[0] = '\0';
  tx.suffix[1] = '\0';
}

bool
init(Client &client) noexcept {
  sp::byte a = 'a';
  sp::byte b = 'a';
  in_order(client.tree, [&client, &a, &b](Tx &tx) {
    tx.prefix[0] = a;
    tx.prefix[1] = b++;
    if (b == sp::byte('z')) {
      ++a;
      b = 'a';
    }
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
  constexpr std::size_t capacity = sizeof(TxTree::storage);

Lstart:
  const std::size_t abs_idx = translate(level, idx);

  if (abs_idx < capacity) {
    Tx &current = tree.storage[abs_idx];
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

        f(tree.storage[translate(level, idx)]);

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
is_expired(const Tx &tx, time_t now) noexcept {
  if (tx.sent != 0) {
    Config config;
    if ((tx.sent + config.transaction_timeout) < now) {
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

TxHandle
take_tx(Client &client, const krpc::Transaction &needle) noexcept {
  constexpr std::size_t c = sizeof(Tx::prefix) + sizeof(Tx::suffix);
  if (needle.length == c) {

    Tx *const tx = search(client.tree, needle);
    if (tx) {

      if (*tx == needle) {
        TxHandle handle = tx->handle;
        reset(*tx);
        move_front(client, tx);

        return handle;
      }
    }
  }

  return nullptr;
} // dht::take_tx()

static Tx *
unlink_free(Client &client, time_t now) noexcept {
  Tx *const head = client.timeout_head;
  if (head) {

    if (is_expired(*head, now)) {

      return unlink(client, head);
    }
  }

  return nullptr;
}

bool
mint_tx(Client &client, krpc::Transaction &out, time_t now,
        TxHandle h) noexcept {
  assert(h);

  Tx *const tx = unlink_free(client, now);
  if (tx) {
    {
      const int r = rand();
      static_assert(sizeof(tx->suffix) <= sizeof(r), "");
      std::memcpy(tx->suffix, &r, sizeof(tx->suffix));
    }

    std::memcpy(out.id, tx->prefix, sizeof(tx->prefix));
    out.length = sizeof(tx->prefix);
    std::memcpy(out.id + out.length, tx->suffix, sizeof(tx->suffix));
    out.length += sizeof(tx->suffix);

    tx->handle = h;
    tx->sent = now;
    add_back(client, tx);

    return true;
  }

  return false;
} // dht::min_transaction()

} // namespace dht
