#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>
#include <time.h>

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
};
} // namespace krpc

//---------------------------
namespace sp {
/*list*/
template <typename T>
struct node {
  node *next;
  T value;

  node()
      : next(nullptr)
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
};

template <typename T, typename F>
static void
for_each(const sp::list<T> &list, F f) noexcept {
  sp::node<T> *l = list.root;
  for (std::size_t i = 0; i < list.size; ++i) {
    f(l->value);
    l = l->next;
  }
}

} // namespace sp

//---------------------------
/*dht*/
namespace dht {
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
  NodeId()
      : id{0} {
  }
};

/*Token*/
struct Token {
  sp::byte id[16];
  Token()
      : id{0} {
  }
};

/*Peer*/
struct Peer {
  Ip ip;
  Port port;
  // {
  Peer *next;
  // }
  Peer(Ip, Port);
  Peer();
};

/*Contact*/
// 15 min refresh
struct Node {
  time_t activity;
  NodeId id;
  Peer peer;
  std::uint8_t ping_outstanding;

  // timeout {{{
  Node *next;
  Node *priv;
  // }}}

  Node();
  Node(const NodeId &, Ip, Port, time_t);
  Node(const NodeId &, const Peer &, time_t);

  explicit operator bool() const noexcept;
};

} // namespace dht

#endif
