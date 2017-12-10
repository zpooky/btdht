#include "client.h"
#include "udp.h"
#include <cassert>
#include <cstring>

namespace dht {
/*dht::Tx*/
Tx::Tx() noexcept
    : handle(nullptr)
    , timeout_next(nullptr)
    , timeout_priv(nullptr)
    , sent(0)
    , prefix{0}
    , suffix{0} {
}

bool
Tx::operator==(const krpc::Transaction &t) const noexcept {
  constexpr std::size_t p_len = sizeof(prefix);
  constexpr std::size_t s_len = sizeof(suffix);

  if (t.length == (p_len + s_len)) {
    if (std::memcmp(t.id, prefix, p_len) == 0) {
      if (std::memcmp(t.id + p_len, suffix, s_len) == 0) {
        return true;
      }
    }
  }
  return false;
}

int
Tx::cmp(const krpc::Transaction &tx) const noexcept {
  // TODO
  return 0;
}

static void
reset(Tx &tx) noexcept {
  tx.handle = nullptr;
  tx.sent = 0;
}

/*dht::TxTree*/
TxTree::TxTree() noexcept
    : storage{} {
}

/*dht::Client*/
Client::Client(fd &fd) noexcept
    : udp(fd)
    , tree()
    , timeout_head(nullptr) {
}

bool
init(Client &) noexcept {
  // TODO
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

TxHandle
take_tx(Client &client, const krpc::Transaction &needle) noexcept {
  if (needle.length > 0) {
    Tx *const tx = search(client.tree, needle);
    if (tx) {
      if (*tx == needle) {
        TxHandle handle = tx->handle;
        reset(*tx);

        return handle;
      }
    }
  }

  return nullptr;
} // dht::take_tx()

static Tx *
next_free(Client &client) noexcept {
  // TODO
  return nullptr;
}

void
timeout_append(Client &client, Tx *) noexcept {
  // TODO
}

bool
mint_transaction(Client &client, krpc::Transaction &out, time_t now) noexcept {
  Tx *const t = next_free(client);
  if (t) {
    {
      std::memcpy(out.id, t->prefix, sizeof(out.id));
      out.length = sizeof(out.id);

      const int r = rand();
      std::memcpy(out.id + out.length, &r, 2);
      out.length += 2;

      std::memcpy(t->suffix, &r, 2);
    }

    t->handle = nullptr; // TODO
    t->sent = now;
    timeout_append(client, t);

    return true;
  }

  return false;
} // dht::min_transaction()

bool
send(Client &c, const Contact &dest, const krpc::Transaction &,
     sp::Buffer &buf) noexcept {
  return udp::send(c.udp, dest, buf);
} // dht::send()

} // namespace dht
