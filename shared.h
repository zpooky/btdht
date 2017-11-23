#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>
#include <time.h>

#include <stdio.h> //debug
#include <stdlib.h>

/*fd*/
class fd {
private:
  int m_fd;

public:
  explicit fd(int p_fd);

  fd(const fd &) = delete;
  fd(fd &&o);
  fd &
  operator=(const fd &) = delete;
  fd &
  operator=(const fd &&) = delete;

  ~fd();

  explicit operator int() noexcept;
};

using Port = std::uint16_t;
using Ip = std::uint32_t;
using Timeout = int;
using Seconds = std::uint32_t;

//---------------------------
namespace sp {
using byte = unsigned char;

/*Buffer*/
struct Buffer {
  byte *raw;
  const std::size_t capacity;
  std::size_t length;
  std::size_t pos;

  Buffer(byte *, std::size_t) noexcept;

  template <std::size_t SIZE>
  explicit Buffer(byte (&buffer)[SIZE]) noexcept
      : Buffer(buffer, SIZE) {
  }

  byte &operator[](std::size_t) noexcept;
};

void
flip(Buffer &) noexcept;

void
reset(Buffer &) noexcept;

byte *
offset(Buffer &) noexcept;

std::size_t
remaining_read(Buffer &) noexcept;

std::size_t
remaining_write(Buffer &) noexcept;

} // namespace sp

//---------------------------
namespace krpc {
struct Transaction {
  sp::byte id[16];
  std::size_t length;
  Transaction();
};
} // namespace krpc

//---------------------------
namespace sp {
/*list*/
template <typename T>
struct node {
  node *next;
  T value;

  explicit node(sp::node<T> *n)
      : next(n)
      , value() {
  }
};

template <typename T>
struct list {
  node<T> *root;
  std::size_t size;
  std::size_t capacity;

  list()
      : root(nullptr)
      , size(0)
      , capacity(0) {
  }

  list(const list<T> &) = delete;
  list(const list<T> &&) = delete;

  list<T> &
  operator=(const list<T> &) = delete;
  list<T> &
  operator=(const list<T> &&) = delete;

  ~list() {
  Lstart:
    if (root) {
      sp::node<T> *next = root->next;
      delete root;
      root = next;
      goto Lstart;
    }
  }
};

template <typename T>
static bool
init(sp::list<T> &l, std::size_t capacity) noexcept {
  sp::node<T> *next = l.root;

  while (l.capacity < capacity) {
    sp::node<T> *c = new sp::node<T>(next);
    if (!c) {
      return false;
    }

    l.root = next = c;
    l.capacity++;
  }
  return true;
}

template <typename T>
static T *
get(sp::list<T> &l, std::size_t idx) noexcept {
  sp::node<T> *current = l.root;
Lstart:
  if (current) {
    if (idx == 0) {
      return &current->value;
    }
    --idx;
    current = current->next;
    goto Lstart;
  }
  return nullptr;
}

template <typename T>
static const T *
get(const sp::list<T> &l, std::size_t idx) noexcept {
  const sp::node<T> *current = l.root;
Lstart:
  if (current) {
    if (idx == 0) {
      return &current->value;
    }
    --idx;
    current = current->next;
    goto Lstart;
  }
  return nullptr;
}

template <typename T>
static bool
push_back(sp::list<T> &list, const T &val) noexcept {
  if (list.size < list.capacity) {
    T *const value = get(list, list.size);
    if (value) {
      *value = val;
      list.size++;
      return true;
    }
  }
  return false;
}

template <typename T, typename F>
void
for_each(const sp::list<T> &list, F f) noexcept {
  sp::node<T> *l = list.root;
  for (std::size_t i = 0; i < list.size; ++i) {
    // assert(l);

    f(l->value);
    l = l->next;
  }
}

template <typename T, typename F>
bool
for_all(const sp::list<T> &list, F f) noexcept {
  sp::node<T> *l = list.root;
  for (std::size_t i = 0; i < list.size; ++i) {
    // assert(l);

    bool r = f(l->value);
    if (!r) {
      return false;
    }
    l = l->next;
  }
  return true;
}

} // namespace sp

//---------------------------
/*dht*/
namespace dht {
struct Config {
  time_t min_timeout_interval;
  time_t refresh_interval;
  time_t peer_age_refresh;

  Config() noexcept;
};

using Key = sp::byte[20];

/*Infohash*/
struct Infohash {
  Key id;
  Infohash()
      : id{0} {
  }
};

/*NodeId*/
struct NodeId {
  Key id;
  NodeId();
};

bool
is_valid(const NodeId &) noexcept;

/*Token*/
struct Token {
  sp::byte id[20];
  Token()
      : id{0} {
  }
};

/*Contact*/
struct Contact {
  Ip ip;
  Port port;
  Contact(Ip, Port) noexcept;
  Contact() noexcept;
};

/*Peer*/
struct Peer {
  Contact contact;
  time_t activity;
  // {
  Peer *next;
  // }
  Peer(Ip, Port, time_t) noexcept;
  Peer() noexcept;
};

template <typename F>
static bool
for_all(const dht::Peer *l, F f) noexcept {
  while (l) {
    if (!f(*l)) {
      return false;
    }
    l = l->next;
  }
  return true;
}

/*Contact*/
// 15 min refresh
struct Node {
  time_t request_activity;
  time_t response_activity;
  time_t ping_sent;
  NodeId id;
  Contact peer;
  std::uint8_t ping_outstanding;

  // timeout {{{
  Node *timeout_next;
  Node *timeout_priv;
  // }}}

  Node() noexcept;
  Node(const NodeId &, Ip, Port, time_t) noexcept;
  Node(const NodeId &, const Contact &, time_t) noexcept;

  explicit operator bool() const noexcept;
};

time_t
activity(const Node &) noexcept;

} // namespace dht

#endif
