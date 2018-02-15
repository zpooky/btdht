#ifndef SP_MALLOC_UTIL_H
#define SP_MALLOC_UTIL_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <list/FixedList.h>

namespace sp {
/*sp::byte*/
using byte = unsigned char;
} // namespace sp

// fd
class fd {
private:
  int m_fd;

public:
  explicit fd(int) noexcept;

  explicit operator bool() const noexcept;

  fd(const fd &) = delete;
  fd(fd &&o) noexcept;

  fd &
  operator=(const fd &) = delete;
  fd &
  operator=(const fd &&) = delete;

  ~fd() noexcept;

  explicit operator int() noexcept;
};

using Port = std::uint16_t;
using Ipv4 = std::uint32_t;

struct Ipv6 {
  sp::byte raw[16];
};

enum class IpType : uint8_t { IPV4, IPV6 };

struct Ip {
  union {
    Ipv4 ipv4;
    Ipv6 ipv6;
  };
  IpType type;

  explicit Ip(Ipv4);
  explicit Ip(const Ipv6 &);

  bool
  operator==(const Ip &) const noexcept;

  bool
  operator<(const Ip &) const noexcept;

  bool
  operator>(const Ip &) const noexcept;
};

// Contact
struct Contact {
  Ip ip;
  Port port;

  Contact(Ipv4, Port) noexcept;
  Contact(const Ipv6 &, Port) noexcept;
  Contact(const Ip &, Port) noexcept;
  // Contact(const Contact &) noexcept;
  Contact() noexcept;

  bool
  operator==(const Contact &) const noexcept;

  bool
  operator<(const Contact &) const noexcept;

  bool
  operator>(const Contact &) const noexcept;
};

bool
convert(const char *, Contact &) noexcept;

bool
to_ipv4(const char *, Ipv4 &) noexcept;

bool
to_string(const Contact &, char *msg, std::size_t) noexcept;

template <std::size_t SIZE>
bool
to_string(const Contact &c, char (&msg)[SIZE]) noexcept {
  return to_string(c, msg, SIZE);
}

bool
convert(const char *, Port &) noexcept;

using Timeout = int;
using Seconds = std::uint32_t;

namespace dht {
// dht::Token
struct Token {
  sp::byte id[20];
  std::size_t length;

  Token() noexcept;

  bool
  operator==(const Token &) const noexcept;
};

bool
is_valid(const Token &) noexcept;

} // namespace dht

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
using list = FixedList<T>;

template <typename T>
using node = impl::LinkedList::Node<T>;
} // namespace sp

//---------------------------
namespace krpc {
// krpc::Transaction
struct Transaction {
  sp::byte id[16];
  std::size_t length;

  Transaction() noexcept;
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

  static constexpr std::size_t bits = sizeof(Key) * 8;

  bool
  operator==(const NodeId &) const noexcept;

  bool
  operator==(const Key &) const noexcept;

  bool
  operator<(const NodeId &o) const noexcept;
};

bool
is_valid(const NodeId &) noexcept;

bool
bit(const Key &key, std::size_t idx) noexcept;

bool
bit(const NodeId &key, std::size_t idx) noexcept;

void
print_id(const NodeId &, std::size_t color = 0, const char *c = "") noexcept;

void
print_hex(const NodeId &) noexcept;

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
  Token his_token;
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
  Node(const NodeId &, const Contact &, time_t) noexcept;
  Node(const Node &, time_t) noexcept;

  explicit operator bool() const noexcept;
};

} // namespace dht

#endif
