#ifndef SP_MAINLINE_DHT_UTIL_H
#define SP_MAINLINE_DHT_UTIL_H

#include <buffer/BytesView.h>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <hash/standard.h>
#include <io/fd.h>
#include <limits.h>
#include <list/FixedList.h>
#include <netinet/in.h>
#include <util/timeout.h>

using sp::fd;

//=====================================
namespace client {
enum class Res { ERR, ERR_TOKEN, OK };
}

//=====================================
namespace sp {
/*sp::byte*/
using byte = unsigned char;

using ByteBuffer = BytesView;
} // namespace sp

//=====================================
using Port = std::uint16_t;

//=====================================
using Ipv4 = std::uint32_t;

bool
to_ipv4(const char *, Ipv4 &) noexcept;

//=====================================
struct Ipv6 {
  sp::byte raw[16];
};

bool
to_ipv6(const char *, Ipv6 &) noexcept;

namespace sp {
template <>
struct Hasher<Ipv6> {
  std::size_t
  operator()(const Ipv6 &) const noexcept;
};
} // namespace sp

//=====================================
enum class IpType : uint8_t { IPV4, IPV6 };

struct Ip {
  union {
    Ipv4 ipv4;
    Ipv6 ipv6;
  };
  IpType type;

  explicit Ip(Ipv4);
  explicit Ip(const Ipv6 &);

  Ip &
  operator=(const Ipv4 &) noexcept;
  Ip &
  operator=(const Ipv6 &) noexcept;

  bool
  operator==(const Ip &) const noexcept;

  bool
  operator<(const Ip &) const noexcept;

  bool
  operator>(const Ip &) const noexcept;
};

namespace sp {
template <>
struct Hasher<Ip> {
  std::size_t
  operator()(const Ip &) const noexcept;
};
} // namespace sp

std::size_t
fnv_ip(const Ip &) noexcept;

std::size_t
djb_ip(const Ip &) noexcept;

//=====================================
struct Contact {
  /*   union { */
  /*     struct { */
  Ip ip;
  Port port;
  /*     } inet; */
  /*   }; */

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

std::size_t
fnv_contact(const Contact &) noexcept;

std::size_t
djb_contact(const Contact &) noexcept;

bool
to_contact(const char *, Contact &) noexcept;

bool
to_contact(const ::sockaddr_in &, Contact &) noexcept;

const char *
to_string(const ::sockaddr_in &) noexcept;

bool
to_contact(const ::in_addr &, Port, Contact &) noexcept;

bool
to_string(const ::in_addr &, char *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
to_string(const ::in_addr &ip, char (&msg)[SIZE]) noexcept {
  return to_string(ip, msg, SIZE);
}

const char *
to_string(const ::in_addr &) noexcept;

bool
to_sockaddr(const Contact &, ::sockaddr_in &) noexcept;

bool
to_string(const Ip &, char *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
to_string(const Ip &ip, char (&msg)[SIZE]) noexcept {
  return to_string(ip, msg, SIZE);
}

const char *
to_string(const Ip &) noexcept;

bool
to_string(const Contact &, char *msg, std::size_t) noexcept;

template <std::size_t SIZE>
bool
to_string(const Contact &c, char (&msg)[SIZE]) noexcept {
  return to_string(c, msg, SIZE);
}

const char *
to_string(const Contact &) noexcept;

bool
to_port(const char *, Port &) noexcept;

using Timestamp = sp::Timestamp;

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
using Buffer = ByteBuffer;

//---------------------------
// sp::list
template <typename T>
using list = FixedList<T>;

template <typename T>
using node = impl::LinkedListNode<T>;
} // namespace sp

//---------------------------
namespace krpc {
// krpc::Transaction
struct Transaction {
  sp::byte id[32];
  std::size_t length;

  Transaction() noexcept;

  template <std::size_t N1, std::size_t N2>
  Transaction(sp::byte (&one)[N1], sp::byte (&two)[N2]) noexcept
      : Transaction() {
    static_assert((N1 + N2) <= sizeof(id), "");
    std::memcpy(id + length, one, N1);
    length += N1;

    std::memcpy(id + length, two, N2);
    length += N2;
  }

  Transaction(const Transaction &o) noexcept {
    memcpy(id, o.id, sizeof(id));
    length = o.length;
  }

  Transaction(const Transaction &&) = delete;

  Transaction &
  operator=(const Transaction &) noexcept;
};
} // namespace krpc

//=====================================
namespace dht {
using Key = sp::byte[20];

std::size_t
rank(const Key &, const Key &) noexcept;

bool
bit(const sp::byte (&key)[20], std::size_t idx) noexcept;

} // namespace dht

//=====================================
namespace dht {
struct Infohash {
  Key id;

  Infohash() noexcept;

  bool
  operator==(const Infohash &) const noexcept;

  bool
  operator>(const Key &) const noexcept;

  bool
  operator>(const Infohash &) const noexcept;
};

bool
from_hex(dht::Infohash &, const char *) noexcept;

bool
to_string(const dht::Infohash &, char *buf, size_t) noexcept;

template <size_t SIZE>
bool
to_string(const dht::Infohash &ih, char (&buf)[SIZE]) noexcept {
  return to_string(ih, buf, SIZE);
}
} // namespace dht

//=====================================
namespace dht {
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

  void
  set_bit(std::size_t, bool) noexcept;
};

std::size_t
rank(const NodeId &, const Key &) noexcept;

std::size_t
rank(const NodeId &id, const NodeId &o) noexcept;

template <std::size_t N>
bool
from_hex(NodeId &id, const char (&b)[N]) noexcept;

bool
from_hex(NodeId &id, const char *b) noexcept;

const char *
to_hex(const NodeId &id) noexcept;

const char *
to_string(const NodeId &) noexcept;

bool
is_valid(const NodeId &) noexcept;

bool
bit(const NodeId &key, std::size_t idx) noexcept;

void
print_id(const NodeId &, std::size_t color = 0, const char *c = "") noexcept;

void
print_hex(const NodeId &) noexcept;

void
print_hex(const krpc::Transaction &tx);

void
print_hex(const sp::byte *arr, std::size_t length, FILE *f = stdout);
} // namespace dht

namespace sp {
template <>
struct Hasher<dht::NodeId> {
  std::size_t
  operator()(const dht::NodeId &) const noexcept;
};
} // namespace sp

//=====================================
namespace dht {
struct IdContact {
  Contact contact;
  NodeId id;
  IdContact() noexcept;
  IdContact(const NodeId &id, const Contact &contact) noexcept;
};

const char *
to_string(const IdContact &) noexcept;
} // namespace dht

//=====================================
namespace dht {
/*valid BEP42 conforming NodeId*/
// XXX
enum class NodeIdValid : std::uint8_t { VALID, NOT_VALID, NOT_YET };

//=====================================
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
  /* When we received request & response from remote */
  Timestamp remote_activity;
  /* When we sent a request to remote */
  Timestamp req_sent;
  //}}}

  //{{{
  std::uint8_t outstanding;
  NodeIdValid valid_id;
  bool good;
  // }}}

  Node() noexcept;
  Node(const NodeId &, const Contact &, Timestamp) noexcept;
  Node(const IdContact &, Timestamp) noexcept;
};

bool
is_valid(const Node &) noexcept;

//=====================================
template <std::size_t N>
bool
from_hex(NodeId &id, const char (&b)[N]) noexcept {
  return from_hex(id, b, N);
}

} // namespace dht

//=====================================
bool
xdg_cache_dir(char (&directory)[PATH_MAX]) noexcept;

bool
xdg_runtime_dir(char (&directory)[PATH_MAX]) noexcept;

//=====================================

#endif
