#include "util.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <memory>
#include <unistd.h> //close

/*fd*/
fd::fd(int p_fd)
    : m_fd(p_fd) {
}

fd::fd(fd &&o)
    : m_fd(o.m_fd) {
  o.m_fd = -1;
}

fd::~fd() {
  if (m_fd > 0) {
    ::close(m_fd);
    m_fd = -1;
  }
}

fd::operator int() noexcept {
  return m_fd;
}

/*Contact*/
Contact::Contact(Ipv4 v4, Port p) noexcept
    : ipv4(v4)
    , port(p)
    , type(IpType::IPV4) {
}

Contact::Contact(const Ipv6 &v6, Port p) noexcept
    : ipv6()
    , port(p)
    , type(IpType::IPV6) {
  std::memcpy(ipv6.raw, v6.raw, sizeof(ipv6));
}

Contact::Contact() noexcept
    : Contact(0, 0) {
}

bool
Contact::operator==(const Contact &c) const noexcept {
  if (type == IpType::IPV4) {
    return ipv4 == c.ipv4 && port == c.port;
  }
  return std::memcmp(ipv6.raw, c.ipv6.raw, sizeof(ipv6.raw)) == 0 &&
         port == c.port;
}

static bool
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
  result.type = IpType::IPV4;
  std::memcpy(ipstr, str, iplen);
  if (!to_ipv4(ipstr, result.ipv4)) {
    return false;
  }

  const char *portstr = col + 1;
  if (!to_port(portstr, result.port)) {
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
to_string(const Contact &ip, char *str, std::size_t length) noexcept {
  if (ip.type == IpType::IPV6) {
    if (length < (INET6_ADDRSTRLEN + 1 + 5 + 1)) {
      return false;
    }

    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(sockaddr_in6));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(ip.port);
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
    addr.sin_addr.s_addr = htonl(ip.ipv4);

    if (inet_ntop(AF_INET, &addr.sin_addr, str, socklen_t(length)) == nullptr) {
      return false;
    }
    std::strcat(str, ":");
    char pstr[6] = {0};
    sprintf(pstr, "%d", ip.port);
    std::strcat(str, pstr);
  }
  return true;
}

//---------------------------
namespace sp {
/*Buffer*/
Buffer::Buffer(byte *s, std::size_t l) noexcept
    : raw(s)
    , capacity(l)
    , length(0)
    , pos(0) {
}

Buffer::Buffer(Buffer &in) noexcept
    : raw(in.raw)
    , capacity(in.capacity)
    , length(in.length)
    , pos(in.pos) {
}

Buffer::Buffer(Buffer &in, std::size_t strt, std::size_t end) noexcept
    : raw(in.raw + strt)
    , capacity(end - strt)
    , length(end - strt)
    , pos(0) {
}

byte &Buffer::operator[](std::size_t idx) noexcept {
  assert(idx < capacity);
  return raw[idx];
}

const byte &Buffer::operator[](std::size_t idx) const noexcept {
  assert(idx < capacity);
  return raw[idx];
}

void
flip(Buffer &b) noexcept {
  std::swap(b.length, b.pos);
}

void
reset(Buffer &b) noexcept {
  b.length = 0;
  b.pos = 0;
}

byte *
offset(Buffer &b) noexcept {
  return b.raw + b.pos;
}

std::size_t
remaining_read(const Buffer &b) noexcept {
  return b.length - b.pos;
}

std::size_t
remaining_write(const Buffer &b) noexcept {
  return b.capacity - b.pos;
}

} // namespace sp
namespace krpc {
/*krpc::Transaction*/
Transaction::Transaction()
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
print_hex(const NodeId &) noexcept {
  // TODO
  printf("\n");
}

bool
bit(const Key &key, std::size_t idx) noexcept {
  assert(idx < NodeId::bits);

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
    , request_activity(0)
    , response_activity(0)
    , ping_sent(0)
    //}}}
    //{{{
    , ping_outstanding(0)
    , valid_id(NodeIdValid::NOT_YET)
    , good(true)
//}}}
{
}

/*Node*/
Node::Node(const NodeId &nid, const Contact &p, time_t act) noexcept
    // timeout{{{
    : timeout_next(nullptr)
    , timeout_priv(nullptr)
    //}}}
    //{{{
    , id(nid)
    , contact(p)
    //}}}
    // activity {{{
    , request_activity(act)
    , response_activity(act) // TODO??
    , ping_sent(act)         // TODO??
    //}}}
    //{{{
    , ping_outstanding(0)
    , valid_id(NodeIdValid::NOT_YET)
    , good(true)
//}}}
{
}

Node::Node(const Node &node, time_t now) noexcept
    : Node(node.id, node.contact, now) {
}

Node::operator bool() const noexcept {
  return is_valid(id);
}

} // namespace dht
