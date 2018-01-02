#ifndef SP_MALLOC_UTIL_H
#define SP_MALLOC_UTIL_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <time.h>

namespace sp {
/*sp::byte*/
using byte = unsigned char;
} // namespace sp

// fd
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
using Ipv4 = std::uint32_t;

struct Ipv6 {
  sp::byte raw[16];
};

enum class IpType : uint8_t { IPV4, IPV6 };
// ExternalIp
struct ExternalIp {
  union {
    Ipv4 v4;
    Ipv6 v6;
  };

  Port port;
  IpType type;

  ExternalIp(Ipv4, Port) noexcept;
  ExternalIp(const Ipv6 &, Port) noexcept;
};

bool
to_ipv4(const char *, Ipv4 &) noexcept;

bool
to_string(const ExternalIp &, char *msg, std::size_t) noexcept;

using Timeout = int;
using Seconds = std::uint32_t;

//---------------------------
namespace sp {
// sp::Buffer
struct Buffer {
  byte *raw;
  const std::size_t capacity;
  std::size_t length;
  std::size_t pos;

  Buffer(byte *, std::size_t) noexcept;
  explicit Buffer(Buffer &) noexcept;
  Buffer(Buffer &, std::size_t, std::size_t) noexcept;

  template <std::size_t SIZE>
  explicit Buffer(byte (&buffer)[SIZE]) noexcept
      : Buffer(buffer, SIZE) {
  }

  byte &operator[](std::size_t) noexcept;
  const byte &operator[](std::size_t) const noexcept;
};

void
flip(Buffer &) noexcept;

void
reset(Buffer &) noexcept;

byte *
offset(Buffer &) noexcept;

std::size_t
remaining_read(const Buffer &) noexcept;

std::size_t
remaining_write(const Buffer &) noexcept;

//---------------------------
// sp::list
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

  using value_type = T;

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
static void
clear(sp::list<T> &l) {
  sp::node<T> *current = l.root;
  std::size_t size = l.size;
Lstart:
  if (current) {
    if (size-- > 0) {
      current->value.~T();
      new (&current->value) T;
      current = current->next;
      goto Lstart;
    }
  }
  size = 0;
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
    assert(l);

    f(l->value);
    l = l->next;
  }
}

template <typename T, typename F>
bool
for_all(const sp::list<T> &list, F f) noexcept {
  sp::node<T> *l = list.root;
  for (std::size_t i = 0; i < list.size; ++i) {
    assert(l);

    bool r = f(l->value);
    if (!r) {
      return false;
    }
    l = l->next;
  }
  return true;
}

template <typename T, typename F>
bool
remove_first(sp::list<T> &list, F f) {
  sp::node<T> *l = list.root;
  sp::node<T> *priv = nullptr;
  for (std::size_t i = 0; i < list.size; ++i) {
    // assert(l);

    if (f(l->value)) {
      l->value.~T();
      new (&l->value) T;

      auto next = l->next;
      if (priv) {
        priv->next = next;
      } else {
        list.root = next;
      }
      list.size--;

      // TODO append end
      return true;
    }
    priv = l;
    l = l->next;
  }
  return false;
}

} // namespace sp

//---------------------------
namespace krpc {
// krpc::Transaction
struct Transaction {
  sp::byte id[16];
  std::size_t length;

  Transaction();
};
} // namespace krpc

//---------------------------
namespace dht {
// dht::Key
using Key = sp::byte[20];

/*dht::NodeId*/
struct NodeId {
  Key id;
  NodeId();

  bool
  operator==(const NodeId &) const noexcept;

  bool
  operator==(const Key &) const noexcept;
};

bool
is_valid(const NodeId &) noexcept;

// dht::Contact
struct Contact {
  Ipv4 ip;
  Port port;

  Contact(Ipv4, Port) noexcept;
  Contact() noexcept;

  bool
  operator==(const Contact &) const noexcept;
};

/*valid BEP42 conforming NodeId*/
// TODO
enum class NodeIdValid : std::uint8_t { VALID, NOT_VALID, NOT_YET };
// dht::Node
struct Node {
  // timeout {{{
  Node *timeout_next;
  Node *timeout_priv;
  // }}}

  //{{{
  NodeId id;
  Contact contact;
  // }}}

  // activity {{{
  time_t request_activity;
  time_t response_activity;
  time_t ping_sent;
  //}}}

  //{{{
  std::uint8_t ping_outstanding;
  NodeIdValid valid_id;
  bool good;
  // }}}

  Node() noexcept;
  Node(const NodeId &, Ipv4, Port, time_t) noexcept;
  Node(const NodeId &, const Contact &, time_t) noexcept;
  Node(const Node &, time_t) noexcept;

  explicit operator bool() const noexcept;
};

} // namespace dht

#endif
