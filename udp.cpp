#include "udp.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <stdio.h>
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket

namespace udp {

static void
die(const char *s) {
  perror(s);
  std::terminate();
}

static void
to_sockaddr(const dht::Contact &src, ::sockaddr_in &dest) noexcept {
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = htonl(src.ip);
  dest.sin_port = htons(src.port);
}

static void
to_peer(const ::sockaddr_in &src, dht::Contact &dest) noexcept {
  dest.ip = ntohl(src.sin_addr.s_addr);
  dest.port = ntohs(src.sin_port);
}

// static void
// recce(fd &udp) noexcept {
//   sockaddr_in s;
//   socklen_t slen = sizeof(sockaddr_in);
//
//   ssize_t recv_len;
//
//   constexpr std::size_t BUFLEN = 2048;
//   sp::byte buf[BUFLEN];
//
//   recv_len = ::recvfrom(int(udp), buf, BUFLEN, 0, (struct sockaddr *)&s,
//   &slen);
//
//   if (recv_len == -1) {
//     die("recvfrom()");
//   }
// }
ExternalIp
local(fd &listen) noexcept {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getsockname(int(listen), saddr, &slen);
  if (ret < 0) {
    die("getsockname()");
  }

  if (saddr->sa_family == AF_INET6) {
    // TODO
    return ExternalIp(0, 0);
  } else {
    Ipv4 ip = ntohl(addr.sin_addr.s_addr);

    return ExternalIp(ip, ntohs(addr.sin_port));
  }
}

fd
bind(Ipv4 ip, Port port) noexcept {
  int udp = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (udp < 0) {
    die("socket()");
  }

  ::sockaddr_in me;
  std::memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(ip);
  ::sockaddr *meaddr = (::sockaddr *)&me;

  int ret = ::bind(udp, meaddr, sizeof(me));
  if (ret < 0) {
    die("bind");
  }
  return fd{udp};
}

static void
receive(int fd, ::sockaddr_in &other, sp::Buffer &buf) noexcept {
  int flag = 0;
  sockaddr *o = (sockaddr *)&other;
  socklen_t slen = sizeof(other);

  sp::byte *const raw = offset(buf);
  std::size_t raw_len = remaining_write(buf);

  ssize_t len = 0;
  do {
    len = ::recvfrom(fd, raw, raw_len, flag, /*OUT*/ o, &slen);
  } while (len < 0 && errno == EAGAIN);

  if (len < 0) {
    die("recvfrom()");
  }
  buf.pos += len;
} // udp::receive()

void
receive(int fd, dht::Contact &other, sp::Buffer &buf) noexcept {
  ::sockaddr_in o;
  receive(fd, o, buf);
  to_peer(o, other);
} // udp::receive()

static bool
send(int fd, ::sockaddr_in &dest, sp::Buffer &buf) noexcept {
  assert(buf.length > 0);
  int flag = 0;
  sockaddr *destaddr = (sockaddr *)&dest;

  sp::byte *const raw = offset(buf);
  const std::size_t raw_len = remaining_read(buf);

  ssize_t sent = 0;
  do {
    sent = ::sendto(fd, raw, raw_len, flag, destaddr, sizeof(dest));
  } while (sent < 0 && errno == EAGAIN);

  if (sent < 0) {
    printf("sent[%zd] = "
           "sendto(fd[%d],raw,raw_len[%zu],flag[%d]),destaddr[%s])\n", //
           sent, int(fd), raw_len, flag, "");
    die("sendto()");
  }

  buf.pos += sent;

  return true;
} // udp::send()

bool
send(int fd, const dht::Contact &dest, sp::Buffer &buf) noexcept {
  ::sockaddr_in d;
  to_sockaddr(dest, d);
  return send(fd, d, buf);
} // udp::send()

bool
send(fd &fd, const dht::Contact &dest, sp::Buffer &buf) noexcept {
  return send(int(fd), dest, buf);
} // udp::send()

} // namespace udp
