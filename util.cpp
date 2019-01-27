#include "util.h"
#include <arpa/inet.h>
#include <cstring>
#include <encode/hex.h>
#include <hash/djb2.h>
#include <hash/fnv.h>
#include <memory>
#include <util/assert.h>

/*Ip*/
Ip::Ip(Ipv4 v4)
    : ipv4(v4)
    , type(IpType::IPV4) {
}

Ip::Ip(const Ipv6 &v6)
    : ipv6()
    , type(IpType::IPV6) {

  std::memcpy(ipv6.raw, v6.raw, sizeof(ipv6));
}

Ip &
Ip::operator=(const Ipv4 &ip) noexcept {
  ipv4 = ip;
  type = IpType::IPV4;
  return *this;
}

Ip &
Ip::operator=(const Ipv6 &ip) noexcept {
  ipv6 = ip;
  type = IpType::IPV6;
  return *this;
}

bool
Ip::operator==(const Ip &ip) const noexcept {
  if (ip.type != type) {
    return false;
  }

  if (ip.type == IpType::IPV4) {
    return ipv4 == ip.ipv4;
  }
  return std::memcmp(ipv6.raw, ip.ipv6.raw, sizeof(ipv6.raw)) == 0;
}

bool
Ip::operator<(const Ip &o) const noexcept {
  if (type == o.type) {
    if (type == IpType::IPV6) {
      return std::memcmp(ipv6.raw, o.ipv6.raw, sizeof(ipv6)) < 0;
    } else {
      return ipv4 < o.ipv4;
    }
  } else if (type == IpType::IPV4) {
    return true;
  }
  return false;
}

bool
Ip::operator>(const Ip &o) const noexcept {
  if (type == o.type) {
    if (type == IpType::IPV6) {
      return std::memcmp(ipv6.raw, o.ipv6.raw, sizeof(ipv6)) > 0;
    } else {
      return ipv4 > o.ipv4;
    }
  } else if (type == IpType::IPV4) {
    return false;
  }
  return true;
}

namespace sp {
std::size_t
Hasher<Ipv6>::operator()(const ::Ipv6 &ip) const noexcept {
  return fnv_1a::encode64(&ip.raw, sizeof(ip.raw));
}

std::size_t
Hasher<Ip>::operator()(const Ip &c) const noexcept {
  if (c.type == IpType::IPV4) {
    Hasher<Ipv4> h;
    return h(c.ipv4);
  } else if (c.type == IpType::IPV6) {
    Hasher<Ipv6> h;
    return h(c.ipv6);
  }

  assertxs(false, (uint8_t)c.type);
  return 0;
}

} // namespace sp

/*Contact*/
Contact::Contact(Ipv4 v4, Port p) noexcept
    : ip(v4)
    , port(p) {
}

Contact::Contact(const Ipv6 &v6, Port p) noexcept
    : ip(v6)
    , port(p) {
}

Contact::Contact(const Ip &i, Port p) noexcept
    : ip(i)
    , port(p) {
}

Contact::Contact() noexcept
    : Contact(0, 0) {
}

bool
Contact::operator==(const Contact &c) const noexcept {
  return ip == c.ip && port == c.port;
}

bool
Contact::operator<(const Contact &o) const noexcept {
  return ip < o.ip;
}

bool
Contact::operator>(const Contact &o) const noexcept {
  return ip > o.ip;
}

static std::uint64_t
fnv_ipv4(const Ipv4 &c) {
  return fnv_1a::encode64(&c, sizeof(c));
}

static std::uint64_t
fnv_ipv6(const Ipv6 &c) {
  return fnv_1a::encode64(c.raw, sizeof(c.raw));
}

std::size_t
fnv_ip(const Ip &c) {
  if (c.type == IpType::IPV4) {
    return fnv_ipv4(c.ipv4);
  } else if (c.type == IpType::IPV6) {
    return fnv_ipv6(c.ipv6);
  }

  assertxs(false, (uint8_t)c.type);
  return 0;
}

static std::uint32_t
djb_ipv4(const Ipv4 &c) {
  return djb2::encode32(&c, sizeof(c));
}

static std::uint32_t
djb_ipv6(const Ipv6 &c) {
  return djb2::encode32(c.raw, sizeof(c.raw));
}

std::size_t
djb_ip(const Ip &c) {
  if (c.type == IpType::IPV4) {
    return djb_ipv4(c.ipv4);
  } else if (c.type == IpType::IPV6) {
    return djb_ipv6(c.ipv6);
  }

  assertxs(false, (uint8_t)c.type);
  return 0;
}

std::size_t
fnv_contact(const Contact &contact) noexcept {
  std::size_t result = fnv_ip(contact.ip);
  return fnv_1a::encode(&contact.port, sizeof(contact.port), result);
}

std::size_t
djb_contact(const Contact &contact) noexcept {
  std::size_t result = djb_ip(contact.ip);
  return djb2::encode(&contact.port, sizeof(contact.port), result);
}

bool
to_contact(const char *str, Contact &result) noexcept {
  const char *col = std::strchr(str, ':');
  if (!col) {
    return false;
  }

  char ipstr[(3 * 4) + 3 + 1] = {0};
  std::size_t iplen = col - str;
  if (iplen + 1 > sizeof(ipstr)) {
    return false;
  }

  // TODO ipv4
  result.ip.type = IpType::IPV4;
  std::memcpy(ipstr, str, iplen);
  if (!to_ipv4(ipstr, result.ip.ipv4)) {
    return false;
  }

  const char *portstr = col + 1;
  if (!to_port(portstr, result.port)) {
    return false;
  }

  return true;
}

bool
to_contact(const ::sockaddr_in &src, Contact &dest) noexcept {
  // TODO ipv4
  dest.ip.ipv4 = ntohl(src.sin_addr.s_addr);
  dest.ip.type = IpType::IPV4;
  dest.port = ntohs(src.sin_port);
  return true;
}

bool
to_sockaddr(const Contact &src, ::sockaddr_in &dest) noexcept {
  assertx(src.ip.type == IpType::IPV4);
  // TODO ipv4
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = htonl(src.ip.ipv4);
  dest.sin_port = htons(src.port);
  return true;
}

bool
to_ipv4(const char *str, Ipv4 &result) noexcept {
  bool ret = inet_pton(AF_INET, str, &result) == 1;
  result = ntohl(result);
  return ret;
}

bool
to_string(const Ip &ip, char *str, std::size_t length) noexcept {
  if (ip.type == IpType::IPV6) {
    if (length < (INET6_ADDRSTRLEN + 1 + 5 + 1)) {
      return false;
    }

    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(sockaddr_in6));
    addr.sin6_family = AF_INET6;
    // addr.sin6_port = htons(c.port);
    // TODO copy over ip

    if (!inet_ntop(AF_INET6, &addr.sin6_addr, str, socklen_t(length))) {
      return false;
    }
  } else {
    if (length < (INET_ADDRSTRLEN + 1 + 5 + 1)) {
      return false;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ip.ipv4);

    if (!inet_ntop(AF_INET, &addr.sin_addr, str, socklen_t(length))) {
      return false;
    }
  }
  return true;
}

bool
to_string(const Contact &c, char *str, std::size_t length) noexcept {
  if (!to_string(c.ip, str, length)) {
    return false;
  }

  std::strcat(str, ":");
  char pstr[6] = {0};
  sprintf(pstr, "%d", c.port);
  std::strcat(str, pstr);
  return true;
}

bool
to_port(const char *str, Port &result) noexcept {
  auto p = std::atoll(str);
  if (p < 0) {
    return false;
  }
  constexpr std::uint16_t max = ~std::uint16_t(0);
  if (p > max) {
    return false;
  }

  result = (Port)p;

  return true;
}

namespace dht {
// dht::Token
Token::Token() noexcept
    : id{0}
    , length(0) {
}

bool
Token::operator==(const Token &o) const noexcept {
  if (length == o.length) {
    return std::memcmp(id, o.id, length) == 0;
  }
  return false;
}

bool
is_valid(const Token &o) noexcept {
  Token zero;
  return std::memcmp(zero.id, o.id, o.length) != 0;
}

} // namespace dht

//---------------------------
namespace krpc {
/*krpc::Transaction*/
Transaction::Transaction() noexcept
    : id()
    , length(0) {
}

Transaction &
Transaction::operator=(const Transaction &o) noexcept {
  memcpy(id, o.id, o.length);
  length = o.length;
  return *this;
}

} // namespace krpc

//---------------------------
namespace dht {
// dht::Infohash
Infohash::Infohash() noexcept
    : id{0} {
}

bool
Infohash::operator==(const Infohash &o) const noexcept {
  return std::memcmp(id, o.id, sizeof(id)) == 0;
}

bool
Infohash::operator>(const Key &o) const noexcept {
  return std::memcmp(id, o, sizeof(id)) > 0;
}

/*NodeId*/
NodeId::NodeId()
    : id{0} {
}

bool
NodeId::operator==(const Key &o) const noexcept {
  return std::memcmp(id, o, sizeof(id)) == 0;
}

bool
NodeId::operator==(const NodeId &o) const noexcept {
  return this->operator==(o.id);
}

bool
NodeId::operator<(const NodeId &o) const noexcept {
  return std::memcmp(id, o.id, sizeof(id)) < 0;
}

std::size_t
rank(const Key &id, const Key &o) noexcept {
  std::size_t i = 0;
  for (; i < NodeId::bits; ++i) {
    if (bit(id, i) != bit(o, i)) {
      return i;
    }
  }

  return i;
}

std::size_t
rank(const NodeId &id, const Key &o) noexcept {
  return rank(id.id, o);
}

std::size_t
rank(const NodeId &id, const NodeId &o) noexcept {
  return rank(id, o.id);
}

static std::size_t
word_index(std::size_t abs_idx) noexcept {
  constexpr std::size_t bits(sizeof(char) * 8);
  return abs_idx / bits;
}

static std::size_t
bit_index(std::size_t abs_idx) noexcept {
  constexpr std::size_t bits(sizeof(char) * 8);
  return abs_idx % bits;
}

static char
mask_out(std::size_t idx) noexcept {
  char result = 0;
  return char(result | char(1)) << idx;
}

void
NodeId::set_bit(std::size_t idx, bool v) noexcept {
  std::size_t wIdx = word_index(idx);
  auto &word = id[wIdx];
  const auto bIdx = bit_index(idx);

  char mask = mask_out(bIdx);
  if (v) {
    word = word | mask;
  } else {
    word = word & (~mask);
  }
}

static bool
from_hex(Key &id, const char *b) noexcept {
  assertx(b);

  bool res = false;
  const auto b_len = std::strlen(b);

  if ((2 * sizeof(id)) == b_len) {
    std::size_t id_len = sizeof(id);
    res = hex::decode(b, id, id_len);
    assertx(res);
    assertxs(id_len == sizeof(id), id_len, sizeof(id));
  }

  return res;
}

bool
from_hex(NodeId &id, const char *b) noexcept {
  return from_hex(id.id, b);
}

bool
from_hex(dht::Infohash &id, const char *b) noexcept {
  return from_hex(id.id, b);
}

const char *
to_hex(const NodeId &id) noexcept {
  constexpr std::size_t sz = (sizeof(Key) * 2) + 1;
  assertx(sz == 41);
  static char buf[sz];
  memset(buf, 0, sz);

  auto len = sz - 1;
  auto res = hex::encode(id.id, sizeof(id.id), buf, len);
  assertx(len < sz);
  buf[len] = '\0';

  assertx(res);
  assertxs(len == sz - 1, len, sz - 1);
  assertxs(std::strlen(buf) == sz - 1, std::strlen(buf), sz - 1);

  return buf;
}

const char *
to_string(const NodeId &id) noexcept {
  constexpr std::size_t bits = sizeof(id.id) * 4;
  // constexpr std::size_t bits = sizeof(id.id) * 8;
  constexpr std::size_t sz = bits + 1;
  static char buf[sz];
  memset(buf, 0, sz);
  std::size_t i = 0;
  for (; i < bits; ++i) {
    buf[i] = bit(id, i) ? '1' : '0';
  }
  buf[i] = '\0';

  return buf;
}
} // namespace dht

namespace sp {
std::size_t
Hasher<dht::NodeId>::operator()(const dht::NodeId &id) const noexcept {
  return fnv_1a::encode64(&id.id, sizeof(id.id));
}
} // namespace sp

namespace dht {
void
print_id(const NodeId &id, std::size_t color, const char *c) noexcept {
  for (std::size_t i = 0; i < NodeId::bits; ++i) {
    bool b = bit(id.id, i);
    if (i <= color) {
      printf(c);
    }
    printf("%d", b ? 1 : 0);
    if (i <= color) {
      printf("\033[0m");
    }
  }
  printf("\n");
}

void
print_hex(const NodeId &id) noexcept {
  for (std::size_t i = 0; i < sizeof(id.id); ++i) {
    printf("%hhX", id.id[i]);
  }
  // TODO
  printf("\n");
}

bool
bit(const Key &key, std::size_t idx) noexcept {
  assertxs(idx < NodeId::bits, idx, NodeId::bits);

  std::size_t byte = idx / 8;
  std::uint8_t bit = idx % 8;
  std::uint8_t high_bit(1 << 7);
  std::uint8_t bitMask = std::uint8_t(high_bit >> bit);
  return key[byte] & bitMask;
}

bool
bit(const NodeId &key, std::size_t idx) noexcept {
  return bit(key.id, idx);
}

bool
is_valid(const NodeId &id) noexcept {
  constexpr Key allzeros = {0};
  return !(id == allzeros);
}

/*Node*/
Node::Node() noexcept
    // timeout{{{
    : timeout_next(nullptr)
    , timeout_priv(nullptr)
    //}}}
    //{{{
    , id()
    , contact()
    //}}}
    , remote_activity(0)
    , req_sent(0)
    //}}}
    //{{{
    , outstanding(0)
    , valid_id(NodeIdValid::NOT_YET)
    , good(true)
//}}}
{
}

/*Node*/
Node::Node(const NodeId &nid, const Contact &p, Timestamp act) noexcept
    // timeout{{{
    : timeout_next(nullptr)
    , timeout_priv(nullptr)
    //}}}
    //{{{
    , id(nid)
    , contact(p)
    , his_token()
    //}}}
    // activity {{{
    , remote_activity(act)
    , req_sent(act) // TODO??
    //}}}
    //{{{
    , outstanding(0)
    , valid_id(NodeIdValid::NOT_YET)
    , good(true)
//}}}
{
}

Node::Node(const Node &node, Timestamp now) noexcept
    : Node(node.id, node.contact, now) {
}

bool
is_valid(const Node &n) noexcept {
  bool res = is_valid(n.id);
  if (res) {
    assertxs(n.timeout_next, n.timeout_next, n.timeout_priv, to_hex(n.id));
    assertxs(n.timeout_priv, n.timeout_priv, n.timeout_next, to_hex(n.id));
  } else {
    assertx(!n.timeout_next);
    assertx(!n.timeout_priv);
  }

  return res;
}

} // namespace dht
