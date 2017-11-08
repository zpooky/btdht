#include "shared.h"
#include <cassert>
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
remaining_read(Buffer &b) noexcept {
  return b.length - b.pos;
}

std::size_t
remaining_write(Buffer &b) noexcept {
  return b.capacity - b.pos;
}
} // namespace sp

//---------------------------
namespace dht {
/*Peer*/
Peer::Peer(Ip i, Port p)
    : ip(i)
    , port(p)
    , next(nullptr) {
}

Peer::Peer()
    : Peer(0, 0) {
}

/*Contact*/
Node::Node()
    : activity()
    , id()
    , peer()
    , ping_await(false)
    , next(nullptr) {
}

Node::Node(const NodeId &nid, Ip ip, Port port, time_t la)
    : activity(la)
    , id(nid)
    , peer(ip, port)
    , ping_await(false) {
}

Node::Node(const NodeId &nid, const Peer &p, time_t act)
    : Node(nid, p.ip, p.port, act) {
}

Node::operator bool() const noexcept {
  return peer.ip == 0;
}

} // namespace dht
