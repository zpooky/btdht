#include "util.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <hash/fnv.h>
#include <memory>

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

bool
convert(const char *str, Contact &result) noexcept {
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
  if (!convert(portstr, result.port)) {
    return false;
  }

  return true;
}

bool
to_ipv4(const char *str, Ipv4 &result) noexcept {
  bool ret = inet_pton(AF_INET, str, &result) == 1;
  result = ntohl(result);
  return ret;
}

bool
to_string(const Contact &c, char *str, std::size_t length) noexcept {
  if (c.ip.type == IpType::IPV6) {
    if (length < (INET6_ADDRSTRLEN + 1 + 5 + 1)) {
      return false;
    }

    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(sockaddr_in6));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(c.port);
    // TODO copy over ip

    if (inet_ntop(AF_INET6, &addr.sin6_addr, str, socklen_t(length)) ==
        nullptr) {
      return false;
    }
  } else {
    if (length < (INET_ADDRSTRLEN + 1 + 5 + 1)) {
      return false;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(c.ip.ipv4);

    if (inet_ntop(AF_INET, &addr.sin_addr, str, socklen_t(length)) == nullptr) {
      return false;
    }
    std::strcat(str, ":");
    char pstr[6] = {0};
    sprintf(pstr, "%d", c.port);
    std::strcat(str, pstr);
  }
  return true;
}

bool
convert(const char *str, Port &result) noexcept {
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
namespace sp {} // namespace sp
namespace krpc {
/*krpc::Transaction*/
Transaction::Transaction() noexcept
    : id()
    , length(0) {
}

} // namespace krpc

//---------------------------
namespace dht {
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
  return std::memcmp(id, o.id, sizeof(id)) == -1;
}

NodeId::operator bool() const noexcept {
  return is_valid(*this);
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

std::size_t
common_bits(const Key &a, const Key &b) noexcept {
  std::size_t common = 0;
  constexpr std::size_t bits(sizeof(Key) * 8);
  for (std::size_t i = 0; i < bits; ++i) {
    if (bit(a, i) == bit(b, i)) {
      ++common;
    } else {
      break;
    }
  }
  return common;
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
    , request_activity(0)
    , response_activity(0)
    , req_sent(0)
    //}}}
    //{{{
    , ping_outstanding(0)
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
    , request_activity(act)
    , response_activity(act) // TODO??
    , req_sent(act)          // TODO??
    //}}}
    //{{{
    , ping_outstanding(0)
    , valid_id(NodeIdValid::NOT_YET)
    , good(true)
//}}}
{
}

Node::Node(const Node &node, Timestamp now) noexcept
    : Node(node.id, node.contact, now) {
}

Node::operator bool() const noexcept {
  return is_valid(id);
}

} // namespace dht
