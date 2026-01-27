#include "util.h"
#include <arpa/inet.h>
#include <cstring>
#include <encode/hex.h>
#include <hash/djb2.h>
#include <hash/fnv.h>
#include <memory>
#include <util/assert.h>

//=====================================
bool
to_ipv4(const char *str, Ipv4 &result) noexcept {
  bool ret = inet_pton(AF_INET, str, &result) == 1;
  result = ntohl(result);
  return ret;
}

//=====================================
bool
Ipv6::operator==(const Ipv6 &o) const noexcept {
  return std::memcmp(raw, o.raw, sizeof(raw)) == 0;
}

bool
Ipv6::operator<(const Ipv6 &o) const noexcept {
  return std::memcmp(raw, o.raw, sizeof(raw)) < 0;
}

bool
Ipv6::operator>(const Ipv6 &o) const noexcept {
  return std::memcmp(raw, o.raw, sizeof(raw)) > 0;
}

//=====================================
bool
to_ipv6(const char *str, Ipv6 &result) noexcept {
  static_assert(sizeof(result.raw) == sizeof(struct in6_addr), "");
  bool ret = inet_pton(AF_INET6, str, &result) == 1;
  return ret;
}

//=====================================
/*Ip*/
Ip::Ip(Ipv4 v4)
    : ipv4(v4)
    , type(IpType::IPV4) {
}
Ip::Ip(in_addr v4)
    : ipv4(htonl(v4.s_addr))
    , type(IpType::IPV4) {
}

#ifdef IP_IPV6
Ip::Ip(const Ipv6 &v6)
    : ipv6()
    , type(IpType::IPV6) {

  std::memcpy(ipv6.raw, v6.raw, sizeof(ipv6));
}
#endif

Ip &
Ip::operator=(const Ipv4 &ip) noexcept {
  ipv4 = ip;
  type = IpType::IPV4;
  return *this;
}

#ifdef IP_IPV6
Ip &
Ip::operator=(const Ipv6 &ip) noexcept {
  ipv6 = ip;
  type = IpType::IPV6;
  return *this;
}
#endif

bool
Ip::operator==(const Ip &o) const noexcept {
  if (o.type != type) {
    return false;
  }

  if (o.type == IpType::IPV4) {
    return ipv4 == o.ipv4;
  }
#ifdef IP_IPV6
  return ipv6 == o.ipv6;
#else
  assertx(false);
  return false;
#endif
}

bool
Ip::operator<(const Ip &o) const noexcept {
  if (type == o.type) {
    if (type == IpType::IPV6) {
#ifdef IP_IPV6
      return ipv6 < o.ipv6;
#else
      assertx(false);
      return false;
#endif
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
#ifdef IP_IPV6
      return ipv6 > o.ipv6;
#else
      assertx(false);
      return false;
#endif
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
Hasher<Ip>::operator()(const Ip &c) const noexcept {
  if (c.type == IpType::IPV4) {
    Hasher<Ipv4> h;
    return h(c.ipv4);
  } else if (c.type == IpType::IPV6) {
#ifdef IP_IPV6
    Hasher<Ipv6> h;
    return h(c.ipv6);
#else
    assertx(false);
#endif
  }

  assertxs(false, (uint8_t)c.type);
  return 0;
}
} // namespace sp

static std::uint64_t
fnv_ipv4(const Ipv4 &c) noexcept {
  return fnv_1a::encode64(&c, sizeof(c));
}

#ifdef IP_IPV6
static std::uint64_t
fnv_ipv6(const Ipv6 &c) noexcept {
  return fnv_1a::encode64(c.raw, sizeof(c.raw));
}
#endif

std::size_t
fnv_ip(const Ip &c) noexcept {
  if (c.type == IpType::IPV4) {
    return fnv_ipv4(c.ipv4);
  } else if (c.type == IpType::IPV6) {
#ifdef IP_IPV6
    return fnv_ipv6(c.ipv6);
#else
    assertx(false);
#endif
  }

  assertxs(false, (uint8_t)c.type);
  return 0;
}

static std::uint32_t
djb_ipv4(const Ipv4 &c) noexcept {
  return djb2::encode32(&c, sizeof(c));
}

static std::uint32_t
djb_ipv6(const Ipv6 &c) noexcept {
  return djb2::encode32(c.raw, sizeof(c.raw));
}

std::size_t
djb_ip(const Ip &c) noexcept {
  if (c.type == IpType::IPV4) {
    return djb_ipv4(c.ipv4);
  } else if (c.type == IpType::IPV6) {
#ifdef IP_IPV6
    return djb_ipv6(c.ipv6);
#else
    assertx(false);
#endif
  }

  assertxs(false, (uint8_t)c.type);
  return 0;
}

//=====================================
namespace sp {
std::size_t
Hasher<Ipv6>::operator()(const ::Ipv6 &ip) const noexcept {
  return fnv_1a::encode64(&ip.raw, sizeof(ip.raw));
}
} // namespace sp

//=====================================
/*Contact*/
Contact::Contact(Ipv4 v4, Port p) noexcept
    : ip(v4)
    , port(p) {
}

#ifdef IP_IPV6
Contact::Contact(const Ipv6 &v6, Port p) noexcept
    : ip(v6)
    , port(p) {
}
#endif

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

std::size_t
serialize_size(const Contact &p) noexcept {
  // TODO ipv4
  assertx(p.ip.type == IpType::IPV4);
  return sizeof(p.ip.ipv4) + sizeof(p.port);
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
  if (src.sin_family == AF_INET) {
    in_addr ipv4 = src.sin_addr;
    Port port = ntohs(src.sin_port);
    return to_contact(ipv4, port, dest);
  } else if (src.sin_family == AF_INET6) {
    assertx(false);
  } else if (src.sin_family == AF_UNIX) {
    return true;
  }
  assertx(false);
  return false;
}

const char *
to_string(const ::sockaddr_in &in) noexcept {
  static char buffer[INET6_ADDRSTRLEN + 1 + 5];

  Contact tmp;
  bool result = to_contact(in, tmp);
  assertx(result);

  result = to_string(tmp, buffer, sizeof(buffer));
  assertx(result);

  return buffer;
}

bool
to_contact(const ::in_addr &src, Port p, Contact &dest) noexcept {
  dest.ip.ipv4 = ntohl(src.s_addr);
  dest.ip.type = IpType::IPV4;
  dest.port = p;
  return true;
}

bool
to_string(const in_addr &ip, char *str, std::size_t len) noexcept {
  assertx(len >= INET_ADDRSTRLEN);
  const char *res = ::inet_ntop(AF_INET, &ip, str, len);
  return res != nullptr;
}

const char *
to_string(const ::in_addr &ip) noexcept {
  static char buffer[INET6_ADDRSTRLEN];

  bool result = to_string(ip, buffer, sizeof(buffer));
  assertx(result);

  return buffer;
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

//=====================================
bool
to_string(const Ip &ip, char *str, std::size_t length) noexcept {
  if (ip.type == IpType::IPV6) {
    if (length < (INET6_ADDRSTRLEN + 1 + 5 + 1)) {
      return false;
    }

    sockaddr_in6 addr{};
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

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ip.ipv4);

    if (!inet_ntop(AF_INET, &addr.sin_addr, str, socklen_t(length))) {
      return false;
    }
  }
  return true;
}

const char *
to_string(const Ip &c) noexcept {
  static char buffer[INET6_ADDRSTRLEN];

  bool res = to_string(c, buffer, sizeof(buffer));
  assertx(res);

  return buffer;
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

const char *
to_string(const Contact &c) noexcept {
  static char buffer[INET6_ADDRSTRLEN + 1 + 5];

  bool res = to_string(c, buffer, sizeof(buffer));
  assertx(res);

  return buffer;
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

//=====================================
namespace dht {
// dht::Token
Token::Token() noexcept
    : id{0}
    , length(0) {
}

Token::Token(const char *dummy) noexcept
    : Token() {
  size_t l = strlen(dummy);
  assertx(l < sizeof(id));
  memcpy(id, dummy, l);
  length = l;
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

//=====================================
namespace krpc {
/*krpc::Transaction*/
Transaction::Transaction() noexcept
    : id()
    , length(0) {
}

Transaction::Transaction(const char *str) noexcept
    : Transaction() {
  size_t l = strlen(str);
  assertx(l < sizeof(id)); // TODO
  memcpy(id, str, l);
  length = l;
  id[length] = '\0';
}

Transaction &
Transaction::operator=(const Transaction &o) noexcept {
  memcpy(id, o.id, o.length);
  length = o.length;
  return *this;
}

bool
Transaction::operator==(const Transaction &t) const noexcept {
  if (length != t.length) {
    return false;
  }
  return memcmp(id, t.id, length) == 0;
}

} // namespace krpc

//=====================================
namespace dht {

const char *
to_hex(const Key &id) noexcept {
  constexpr std::size_t sz = (sizeof(Key) * 2) + 1;
  assertx(sz == 41);
  static char buf[sz];
  memset(buf, 0, sz);

  auto len = sz - 1;
  auto res = hex::encode(id, sizeof(id), buf, len);
  assertx(len < sz);
  buf[len] = '\0';

  assertx(res);
  assertxs(len == sz - 1, len, sz - 1);
  assertxs(std::strlen(buf) == sz - 1, std::strlen(buf), sz - 1);
  return buf;
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

bool
bit(const sp::byte (&key)[20], std::size_t idx) noexcept {
  assertxs(idx < NodeId::bits, idx, NodeId::bits);
  constexpr std::uint8_t high_bit(1 << 7);
  const std::size_t byte = idx / 8;
  const std::uint8_t bit = idx % 8;
  const std::uint8_t bitMask(high_bit >> bit);
  return key[byte] & bitMask;
}
} // namespace dht

//=====================================
namespace dht {
// dht::Infohash
Infohash::Infohash() noexcept
    : id{0} {
}

Infohash::Infohash(const char *dummy) noexcept
    : id{0} {
  size_t l = strlen(dummy);
  assertxs(l == sizeof(id), l, sizeof(id));
  memcpy(id, dummy, l);
}

bool
Infohash::operator==(const Infohash &o) const noexcept {
  return std::memcmp(id, o.id, sizeof(id)) == 0;
}

bool
Infohash::operator>(const Key &o) const noexcept {
  return std::memcmp(id, o, sizeof(id)) > 0;
}

bool
Infohash::operator>(const Infohash &o) const noexcept {
  return operator>(o.id);
}

bool
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
from_hex(dht::Infohash &id, const char *b) noexcept {
  return from_hex(id.id, b);
}

void
print_hex(FILE *f, const Infohash &ih) noexcept {
  char out[(sizeof(ih.id) * 2) + 1] = {0};
  assertx_n(hex::encode(ih.id, sizeof(ih.id), out));
  fprintf(f, "%s\n", out);
}

bool
to_string(const dht::Infohash &ih, char *buf, size_t len) noexcept {
  const size_t cap = len;
  if (hex::encode(ih.id, sizeof(ih.id), buf, len)) {
    if (len < cap) {
      buf[len] = '\0';
      return true;
    }
  }

  return false;
}

const char *
to_string(const dht::Infohash &ih) noexcept {
  static char buf[2 * sizeof(ih.id) + 1];
  std::memset(buf, 0, sizeof(buf));
  to_string(ih, buf, sizeof(buf));
  return buf;
}

} // namespace dht

//=====================================
namespace dht {
/*NodeId*/
NodeId::NodeId()
    : id{0} {
}

NodeId::NodeId(const char *dummy)
    : id{0} {
  std::size_t lx = strlen(dummy);
  assertxs(lx == sizeof(id), lx, sizeof(id));
  memcpy(id, dummy, std::min(lx, sizeof(id)));
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

static sp::byte
mask_out(std::size_t idx) noexcept {
  assertx(idx < 8);
  sp::byte result = 0;
  return sp::byte(result | (1 << idx));
}

void
NodeId::set_bit(std::size_t idx, bool v) noexcept {
  std::size_t wIdx = word_index(idx);
  sp::byte &word = this->id[wIdx];
  const auto bIdx = bit_index(idx);

  sp::byte mask = mask_out(bIdx);
  if (v) {
    word = word | mask;
  } else {
    word = word & (~mask);
  }
}

bool
from_hex(NodeId &id, const char *b) noexcept {
  return from_hex(id.id, b);
}

const char *
to_hex(const NodeId &id) noexcept {
  return to_hex(id.id);
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
      printf("%s", c);
    }
    printf("%d", b ? 1 : 0);
    if (i <= color) {
      printf("\033[0m");
    }
  }
  printf("\n");
}

void
print_hex(FILE *f, const NodeId &id) noexcept {
  char out[41] = {0};
  assertx_n(hex::encode(id.id, sizeof(id.id), out));
  fprintf(f, "%s\n", out);
}

void
print_hex(FILE *f, const sp::byte *arr, std::size_t length) {
  const sp::byte *it = arr;
  const sp::byte *const end = it + length;
  char buf[257] = {'\0'};
  while (it != end) {
    size_t buf_len = sizeof(buf) - 1;
    it = hex::encode_inc(it, end, buf, buf_len);
    buf[buf_len] = '\0';
    fprintf(f, "%s", buf);
  }

  // /* TODO convert to use hex::encode */
  // const std::size_t hex_cap = 4096;
  // char hexed[hex_cap + 1] = {0};
  //
  // std::size_t hex_length = 0;
  // std::size_t i = 0;
  // while (i < length && hex_length < hex_cap) {
  //   char buff[128];
  //   std::size_t buffLength = sprintf(buff, "%02x", arr[i++]);
  //   std::memcpy(hexed + hex_length, buff, buffLength);
  //
  //   hex_length += buffLength;
  // }
  //
  // if (i == length) {
  //   printf("%s", hexed);
  // } else {
  //   printf("abbriged[%zu],hex[%zu]:%s", length, i, hexed);
  // }
}

void
print_hex(FILE *f, const krpc::Transaction &tx) {
  print_hex(f, tx.id, tx.length);
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
} // namespace dht

//=====================================
namespace dht {
IdContact::IdContact() noexcept
    : contact{}
    , id{} {
}
IdContact::IdContact(const NodeId &iid, const Contact &icon) noexcept
    : contact(icon)
    , id{iid} {
}

bool
IdContact::operator==(const IdContact &o) const noexcept {
  return this->contact == o.contact && this->id == o.id;
}

const char *
to_string(const IdContact &c) noexcept {
  static char buffer[sizeof(c.id.id) + 1 + INET6_ADDRSTRLEN + 1 + 5] = {0};
  size_t len = sizeof(buffer);
  hex::encode(c.id.id, sizeof(c.id.id), buffer, len);
  strcat(buffer, "#");
  len++;
  to_string(c.contact, buffer + len, sizeof(buffer) - len);

  return buffer;
}
} // namespace dht

//=====================================
namespace dht {
/*Node*/
Node::Node() noexcept
    // timeout{{{
    : timeout_next(nullptr)
    , timeout_priv(nullptr)
    //}}}
    // activity {{{
    , remote_activity(0)
    , req_sent(0) // TODO??
    //}}}
    //{{{
    , contact()
    , id()
    //}}}
    //}}}
    //{{{
    , outstanding(0)
    , properties{0} //}}}
{
  properties.is_good = true;
}

Node::Node(const NodeId &nid, const Contact &p) noexcept
    // timeout{{{
    : timeout_next(nullptr)
    , timeout_priv(nullptr)
    //}}}
    // activity {{{
    , remote_activity(0)
    , req_sent(0) // TODO??
    //}}}
    //{{{
    , contact(p)
    , id(nid)
    //}}}
    //}}}
    //{{{
    , outstanding(0)
    , properties{0} //}}}
{
  properties.is_good = true;
}

/*Node*/
Node::Node(const NodeId &nid, const Contact &p, const Timestamp &act) noexcept
    // timeout{{{
    : timeout_next(nullptr)
    , timeout_priv(nullptr)
    //}}}
    // activity {{{
    , remote_activity(act)
    , req_sent(act) // TODO??
    //}}}
    //{{{
    , contact(p)
    , id(nid)
    //}}}
    //{{{
    , outstanding(0)
    , properties{0} //}}}
{
  properties.is_good = true;
}

Node::Node(const IdContact &node, Timestamp now) noexcept
    : Node(node.id, node.contact, now) {
}

Node::~Node() noexcept {
  assertx(!this->timeout_next);
  assertx(!this->timeout_priv);
  this->timeout_next = nullptr;
  this->timeout_priv = nullptr;
}

#if 0
bool
Node::operator==(const Node &) const noexcept {
  return true;
}
#endif

bool
Node::operator==(const IdContact &o) const noexcept {
  return o == (*this);
}

bool
operator==(const IdContact &f, const Node &s) noexcept {
  return f.contact == s.contact && f.id == s.id;
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

// ========================================
/*dht::Peer*/
Peer::Peer(Ipv4 i, Port p, const Timestamp &n, bool s) noexcept
    : contact(i, p)
    , activity(n)
    , seed(s)
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

Peer::Peer(const Contact &c, const Timestamp &a, bool s) noexcept
    : contact(c)
    , activity(a)
    , seed(s)
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

// Peer::Peer() noexcept
//     : Peer(0, 0, Timestamp(0), false) {
// }

bool
Peer::operator==(const Contact &c) const noexcept {
  return contact.operator==(c);
}

bool
Peer::operator>(const Contact &p) const noexcept {
  return contact > p;
}

bool
Peer::operator>(const Peer &p) const noexcept {
  return operator>(p.contact);
}

bool
operator>(const Contact &f, const Peer &s) noexcept {
  return f > s.contact;
}

Timestamp
activity(const Node &head) noexcept {
  return Timestamp(head.remote_activity);
}

Timestamp
activity(const Peer &peer) noexcept {
  return Timestamp(peer.activity);
}

// ========================================
/*dht::Config*/
Config::Config() noexcept
    : min_timeout_interval(5)
    , refresh_interval(sp::Minutes(15))
    , peer_age_refresh(45)
    , token_max_age(15)
    , transaction_timeout(1)
    //
    , bootstrap_generation_max(16)
    , percentage_seek(50)
    //
    , bucket_find_node_spam(1)
    , max_bucket_not_find_node(5)
    //
    , db_samples_refresh_interval(60)
    //
    , token_key_refresh(15)
    //
    , bootstrap_reset(60)
//
{
}

// ========================================
TokenKey::TokenKey() noexcept
    : key{}
    , created{0} {
}

//=====================================
get_peers_context::~get_peers_context() {
}

ScrapeContext::ScrapeContext(const dht::Infohash &ih)
    : infohash(ih) {
}

//=====================================
bool
support_sample_infohashes(const sp::byte version[DHT_VERSION_LEN]) noexcept {
  if (!version) {
    return false;
  }
  if (std::memcmp(version, (const sp::byte *)"LT", 2) == 0) {
    /* # yes
     * 0x0207
     * 0x0206
     * 0x012F
     * 0x012E
     * 0x012B
     * 0x0102
     * # not
     * 0x0102
     * 0x0101
     * 0x0100
     * 0x0010
     * 0x000f
     */
    return true;
  }
  if (std::memcmp(version, (const sp::byte *)"ml", 2) == 0) {
    // 4:hex[6D6C0109]: 4(ml__)
    // 4:hex[6D6C010B]: 4(ml__)
    return true;
  }
  if (std::memcmp(version, (const sp::byte *)"sp", 2) == 0) {
    return true;
  }
#if 0
  if (std::memcmp(version, (const sp::byte *)"\0\0", 2) == 0) {
    return false;
  }
  if (std::memcmp(version, (const sp::byte *)"lt", 2) == 0) {
    return false;
  }
  if (std::memcmp(version, (const sp::byte *)"UT", 2) == 0) {
    return false;
  }
#endif

  return false;
}
} // namespace dht

//=====================================
// $XDG_CACHE_HOME default equal to $HOME/.cache

bool
xdg_share_dir(char (&directory)[PATH_MAX]) noexcept {
  // read env $XDG_DATA_HOME default to $HOME/.local/share
  const char *data = getenv("XDG_DATA_HOME");
  if (data == NULL || strcmp(data, "") == 0) {
    const char *home = getenv("HOME");
    assertx(home);
    home = home ? home : "/tmp";
    directory[0] = '\0';
    snprintf(directory, PATH_MAX, "%s/.local/share", home);
    return true;
  }

  strcpy(directory, data);
  return true;
}

bool
xdg_runtime_dir(char (&directory)[PATH_MAX]) noexcept {
  // XDG_RUNTIME_DIR (/run/user/1000)
  const char *data = getenv("XDG_RUNTIME_DIR");
  if (data == NULL || strcmp(data, "") == 0) {
    strcpy(directory, "/tmp");
    return true;
  }

  strcpy(directory, data);
  return true;
}

//=====================================
