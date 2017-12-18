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

/*ExternalIp*/
ExternalIp::ExternalIp(Ipv4 ipv4, Port p) noexcept
    : v4(ipv4)
    , port(p)
    , type(IpType::IPV4) {
}

ExternalIp::ExternalIp(const Ipv6 &ipv6, Port p) noexcept
    : v6()
    , port(p)
    , type(IpType::IPV6) {
  std::memcpy(v6.raw, ipv6.raw, sizeof(v6));
}

bool
to_string(const ExternalIp &ip, char *str, std::size_t length) noexcept {
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
    addr.sin_addr.s_addr = htonl(ip.v4);

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

/*Contact*/
Contact::Contact(Ipv4 i, Port p) noexcept
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

/*Node*/
Node::Node() noexcept
    : request_activity(0)
    , response_activity(0)
    , ping_sent(0)
    , id()
    , contact()
    , ping_outstanding(0)
    // timeout{{{
    , timeout_next(nullptr)
    , timeout_priv(nullptr)
//}}}
{
}

/*Node*/
Node::Node(const NodeId &nid, Ipv4 ip, Port port, time_t la) noexcept
    : request_activity(la)
    , response_activity(la) // TODO??
    , ping_sent(la)         // TODO??
    , id(nid)
    , contact(ip, port)
    , ping_outstanding(0)
    // timeout{{{
    , timeout_next(nullptr)
    , timeout_priv(nullptr)
//}}}
{
}

Node::Node(const NodeId &nid, const Contact &p, time_t act) noexcept
    : Node(nid, p.ip, p.port, act) {
}

Node::Node(const Node &node, time_t now) noexcept
    : Node(node.id, node.contact, now) {
}

Node::operator bool() const noexcept {
  return contact.ip == 0;
}

} // namespace dht
