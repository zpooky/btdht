#ifndef SP_MAINLINE_DHT_UTIL_H
#define SP_MAINLINE_DHT_UTIL_H

#include <buffer/BytesView.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
#define DHT_VERSION_LEN 5

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

  bool
  operator==(const Ipv6 &) const noexcept;

  bool
  operator<(const Ipv6 &) const noexcept;

  bool
  operator>(const Ipv6 &) const noexcept;
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

// #define IP_IPV6
struct Ip {
  union {
    Ipv4 ipv4;
#ifdef IP_IPV6
    Ipv6 ipv6;
#endif
  };
  IpType type;

  explicit Ip(Ipv4);
#ifdef IP_IPV6
  explicit Ip(const Ipv6 &);
#endif

  Ip &
  operator=(const Ipv4 &) noexcept;
#ifdef IP_IPV6
  Ip &
  operator=(const Ipv6 &) noexcept;
#endif

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
  Ip ip;
  Port port;

  Contact(Ipv4, Port) noexcept;
#ifdef IP_IPV6
  Contact(const Ipv6 &, Port) noexcept;
#endif
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
serialize_size(const Contact &p) noexcept;

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
  Token(const char *) noexcept;

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
  Transaction(const char *) noexcept;

  template <std::size_t N1, std::size_t N2>
  Transaction(sp::byte (&one)[N1], sp::byte (&two)[N2]) noexcept
      : id{0}
      , length{0} {
    static_assert((N1 + N2) <= sizeof(id), "");
    std::memcpy(id + length, one, N1);
    length += N1;

    std::memcpy(id + length, two, N2);
    length += N2;
  }

  Transaction(const Transaction &o) noexcept
      : Transaction() {
    memcpy(id, o.id, sizeof(id));
    length = o.length;
  }

  Transaction(Transaction &&o) noexcept
      : Transaction(o) {
  }

  Transaction &
  operator=(const Transaction &) noexcept;
  bool
  operator==(const Transaction &) const noexcept;
};
} // namespace krpc

//=====================================
namespace dht {
using Key = sp::byte[20];

const char *
to_hex(const Key &) noexcept;

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
  Infohash(const char *) noexcept;

  bool
  operator==(const Infohash &) const noexcept;

  bool
  operator>(const Key &) const noexcept;

  bool
  operator>(const Infohash &) const noexcept;
};

bool
from_hex(Key &id, const char *) noexcept;

bool
from_hex(dht::Infohash &, const char *) noexcept;

void
print_hex(FILE *f, const Infohash &) noexcept;

bool
to_string(const dht::Infohash &, char *buf, size_t) noexcept;

template <size_t SIZE>
bool
to_string(const dht::Infohash &ih, char (&buf)[SIZE]) noexcept {
  return to_string(ih, buf, SIZE);
}

const char *
to_string(const dht::Infohash &ih) noexcept;
} // namespace dht

//=====================================
namespace dht {
struct NodeId {
  Key id;
  NodeId();
  NodeId(const char *test);

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
print_hex(FILE *f, const NodeId &) noexcept;

void
print_hex(FILE *f, const krpc::Transaction &tx);

void
print_hex(FILE *f, const sp::byte *arr, std::size_t length);
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

  bool
  operator==(const IdContact &o) const noexcept;
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

  // activity {{{
  /* When we received request or response from remote */
  Timestamp remote_activity;
  /* When we sent a request to remote */
  Timestamp req_sent;
  //}}}

  //{{{
  Contact contact;
  NodeId id;
  // }}}

  //{{{
  std::uint8_t outstanding;

  NodeIdValid valid_id; // TODO Bitmask flag
  bool good;            // TODO Bitmask flag
  bool read_only;       // TODO Bitmask flag
  // }}}

  sp::byte version[DHT_VERSION_LEN];

  Node() noexcept;
  Node(const NodeId &, const Contact &) noexcept;
  Node(const NodeId &, const Contact &, const Timestamp &) noexcept;
  Node(const IdContact &, Timestamp) noexcept;

#if 0
  bool
  operator==(const Node &) const noexcept;
#endif

  bool
  operator==(const IdContact &) const noexcept;
};

bool
operator==(const IdContact &f, const Node &s) noexcept;

template <typename F>
bool
for_all(const dht::Node *list[], std::size_t length, F f) noexcept {
  for (std::size_t i = 0; i < length; ++i) {
    if (list[i]) {
      if (!f(*list[i])) {
        return false;
      }
    }
  }
  return true;
}

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
namespace dht {
struct KContact {
  /* number of common prefix bits */
  std::size_t common;
  Contact contact;

  KContact() noexcept
      : common(~std::size_t(0))
      , contact() {
  }

  KContact(std::size_t c, Contact con) noexcept
      : common(c)
      , contact(con) {
  }

  KContact(const Key &in, const Contact &c, const Key &search) noexcept
      : common(rank(in, search))
      , contact(c) {
  }

  KContact(const dht::IdContact &in, const Key &search) noexcept
      : common(rank(in.id, search))
      , contact(in.contact) {
  }

  KContact(const dht::IdContact &in, const NodeId &search) noexcept
      : KContact(in, search.id) {
  }

  explicit
  operator bool() const noexcept {
    return common != ~std::size_t(0);
  }

  bool
  operator>(const KContact &o) const noexcept {
    assertx(o.common != ~std::size_t(0));
    assertx(common != ~std::size_t(0));
    return common > o.common;
  }
};

//=====================================
// dht::Peer
struct Peer {
  Contact contact;
  Timestamp activity;
  bool seed;
  // // {
  // Peer *next;
  // // }
  // {
  Peer *timeout_priv;
  Peer *timeout_next;
  // }
  Peer(Ipv4, Port, const Timestamp &, bool) noexcept;
  Peer(const Contact &, const Timestamp &, bool) noexcept;
  // Peer() noexcept;

  // Peer(const Peer &) = delete;
  // Peer(const Peer &&) = delete;

  // Peer &
  // operator=(const Peer &) = delete;
  // Peer &
  // operator=(const Peer &&) = delete;

  bool
  operator==(const Contact &) const noexcept;

  bool
  operator>(const Contact &) const noexcept;

  bool
  operator>(const Peer &) const noexcept;
};

bool
operator>(const Contact &, const Peer &) noexcept;

Timestamp
activity(const Node &) noexcept;

Timestamp
activity(const Peer &) noexcept;

//=====================================
// dht::Config
struct Config {
  /* Minimum Node refresh await timeout
   */
  sp::Minutes min_timeout_interval;
  /* Node refresh interval
   */
  sp::Milliseconds refresh_interval;
  /* the max age of a peer db entry before it gets evicted */
  sp::Minutes peer_age_refresh;
  sp::Minutes token_max_age;
  /* Max age of transaction created for outgoing request. Used when reclaiming
   * transaction id. if age is greater than max age then we can reuse the
   * transaction.
   */
  sp::Minutes transaction_timeout;
  /* the generation of find_node request sent to bootstrap our routing table.
   * When max generation is reached we start from zero again. when generation is
   * zero we send find_node(self) otherwise we randomize node id
   */
  std::uint8_t bootstrap_generation_max;
  /* A low water mark indecating when we need to find_nodes to supplement nodes
   * present in our RoutingTable. Is used when comparing active with total
   * nodes if there are < percentage active nodes than /percentage_seek/ we
   * start a search for more nodes.
   */
  std::size_t percentage_seek;
  /* The interval of when a bucket can be used again to send find_node request
   * during the 'look_for_nodes' stage.
   */
  sp::Minutes bucket_find_node_spam;
  /* the number of times during 'look_for_nodes' stage a random bucket is
   * selected and was not used to perform find_node because either it did not
   * contain any good nodes or the bucket where too recently used by
   * 'look_for_nodes'
   */
  std::size_t max_bucket_not_find_node;

  sp::Minutes db_samples_refresh_interval;

  /*  */
  sp::Minutes token_key_refresh;
  /*  */
  sp::Minutes bootstrap_reset;

  Config() noexcept;
  Config(const Config &) = delete;
};

// ========================================
struct TokenKey {
  uint32_t key;
  Timestamp created;
  TokenKey() noexcept;
};

// ========================================
struct get_peers_context {
  int dummy;
  virtual ~get_peers_context();
};

struct ScrapeContext : get_peers_context {
  dht::Infohash infohash;
  ScrapeContext(const dht::Infohash &infohash);
};

//=====================================

} // namespace dht

#endif
