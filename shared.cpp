#include "shared.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdio.h>
#include <unistd.h> //close
#include <utility>

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
Transaction::Transaction()
    : id()
    , length(0) {
}

} // namespace krpc

//---------------------------
namespace dht {
/*Config*/
Config::Config() noexcept
    // seconds
    : min_timeout_interval(60)
    , refresh_interval(15 * 60)
    , peer_age_refresh(60 * 45)
    , token_max_age(15 * 60) {
}

/*NodeId*/
NodeId::NodeId()
    : id{0} {
}

bool
is_valid(const NodeId &id) noexcept {
  constexpr Key allzeros = {0};
  return std::memcmp(id.id, allzeros, sizeof(allzeros)) != 0;
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

/*Peer*/
Peer::Peer(Ipv4 i, Port p, time_t n) noexcept
    : contact(i, p)
    , activity(n)
    //{
    , next(nullptr)
    //}
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}
Peer::Peer(const Contact &c, time_t a, Peer *nxt) noexcept
    : contact(c)
    , activity(a)
    //{
    , next(nxt)
    //}
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

Peer::Peer() noexcept
    : Peer(0, 0, 0) {
}

bool
Peer::operator==(const Contact &c) const noexcept {
  return contact.operator==(c);
}

/*Contact*/
Node::Node() noexcept
    : request_activity(0)
    , response_activity(0)
    , ping_sent(0)
    , id()
    , peer()
    , ping_outstanding(0)
    // timeout{{{
    , timeout_next(nullptr)
    , timeout_priv(nullptr)
//}}}
{
}

Node::Node(const NodeId &nid, Ipv4 ip, Port port, time_t la) noexcept
    : request_activity(la)
    , response_activity(la) // TODO??
    , ping_sent(la)         // TODO??
    , id(nid)
    , peer(ip, port)
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
    : Node(node.id, node.peer, now) {
}

Node::operator bool() const noexcept {
  return peer.ip == 0;
}

time_t
activity(const Node &head) noexcept {
  return std::max(head.request_activity, head.response_activity);
}

} // namespace dht
