#include "bencode_print.h"
#include "udp.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <util/assert.h>
// #include <exception>
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket
#include <unistd.h>     //close

namespace udp {
//=====================================
static void
die(const char *s) {
  perror(s);
  std::terminate();
}

static void
to_sockaddr(const Contact &src, ::sockaddr_in &dest) noexcept {
  assertx(src.ip.type == IpType::IPV4);
  // TODO ipv4
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = htonl(src.ip.ipv4);
  dest.sin_port = htons(src.port);
}

static void
to_peer(const ::sockaddr_in &src, Contact &dest) noexcept {
  // TODO ipv4
  dest.ip.ipv4 = ntohl(src.sin_addr.s_addr);
  dest.ip.type = IpType::IPV4;
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
//=====================================
bool
local(fd &listen, Contact &out) noexcept {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getsockname(int(listen), saddr, &slen);
  if (ret < 0) {
    return false;
  }

  if (saddr->sa_family == AF_INET6) {
    assertx(false);
    // TODO
    out = Contact(0, 0);
    return true;
  }

  assertxs(saddr->sa_family == AF_INET, saddr->sa_family);
  Ipv4 ip = ntohl(addr.sin_addr.s_addr);
  Port port = ntohs(addr.sin_port);

  out = Contact(ip, port);
  return true;
}

//=====================================
fd
bind(Ipv4 ip, Port port, Mode mode) noexcept {
  int type = SOCK_DGRAM;
  if (mode == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  int udp = ::socket(AF_INET, type, IPPROTO_UDP);
  if (udp < 0) {
    return fd{-1};
  }

  ::sockaddr_in me;
  std::memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(ip);
  ::sockaddr *meaddr = (::sockaddr *)&me;

  int ret = ::bind(udp, meaddr, sizeof(me));
  if (ret < 0) {
    ::close(udp);
    return fd{-1};
  }

  return fd{udp};
}

fd
bind_v4(Port port, Mode m) noexcept {
  return bind(INADDR_ANY, port, m);
}

fd
bind_v4(Mode m) noexcept {
  return bind_v4(Port(0), m);
}

fd
bind(Ipv6 ip, Port port, Mode mode) noexcept {
  int type = SOCK_DGRAM;
  int ret;
  if (mode == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  int udp = ::socket(AF_INET6, type, IPPROTO_UDP);
  if (udp < 0) {
    return fd{-1};
  }

  int one = 1;
  ret = ::setsockopt(udp, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
  if (ret < 0) {
    ::close(udp);
    return fd{-1};
  }

  ::sockaddr_in6 me;
  std::memset(&me, 0, sizeof(me));
  me.sin6_family = AF_INET6;
  me.sin6_port = htons(port);
  memcpy(&me.sin6_addr, ip.raw, sizeof(ip.raw));
  ::sockaddr *meaddr = (::sockaddr *)&me;

  ret = ::bind(udp, meaddr, sizeof(me));
  if (ret < 0) {
    ::close(udp);
    return fd{-1};
  }

  return fd{udp};
}

// fd
// bind_v6(Port p, Mode m) noexcept {
//   return bind(INADDR6_ANY, p, m);
// }
//
// fd
// bind_v6(Mode m) noexcept {
//   return bind_v6(Port(0), m);
// }
//=====================================
static int
receive(int fd, ::sockaddr_in &other, sp::Buffer &buf) noexcept {
  int flag = 0;
  sockaddr *o = (sockaddr *)&other;
  socklen_t slen = sizeof(other);

  sp::byte *const raw = offset(buf);
  std::size_t raw_len = remaining_write(buf);

  ssize_t len = 0;
  len = ::recvfrom(fd, raw, raw_len, flag, /*OUT*/ o, &slen);
  int err = errno;

  if (len <= 0) {
    return err;
  }

  buf.pos += len;
  return 0;
} // udp::receive()

int
receive(int fd, /*OUT*/ Contact &other, sp::Buffer &buf) noexcept {
  ::sockaddr_in o;

  int res = receive(fd, o, buf);
  if (res == 0) {
    to_peer(o, other);
  }

  return res;
} // udp::receive()

int
receive(fd &udp, /*OUT*/ Contact &c, /*OUT*/ sp::Buffer &b) noexcept {
  return receive(int(udp), c, b);
}

//=====================================
static bool
send(int fd, ::sockaddr_in &dest, sp::Buffer &buf) noexcept {
  assertx(buf.length > 0);
  int flag = 0;
  ::sockaddr *destaddr = (::sockaddr *)&dest;

  int error = 0;
  ssize_t sent = 0;
  do {
    // sp::bencode_print(buf);
    sp::byte *const raw = offset(buf);
    const std::size_t raw_len = remaining_read(buf);
    assertx(raw_len > 0);

    sent = ::sendto(fd, raw, raw_len, flag, destaddr, sizeof(dest));
    error = errno;
    if (sent > 0) {
      buf.pos += sent;
    }

  } while ((sent < 0 && error == EAGAIN) && remaining_read(buf) > 0);

  if (sent < 0) {
    const std::size_t raw_len = remaining_read(buf);
    char dstr[128] = {0};
    socklen_t s_len(sizeof(dstr));
    const char *res = ::inet_ntop(AF_INET, &dest, dstr, s_len);
    assertx(res);

    printf("sent[%zd] = "
           "sendto(fd[%d],raw,raw_len[%zu],flag[%d]),destaddr[%s]): %s\n", //
           sent,                                                           //
           int(fd), raw_len, flag, dstr, strerror(error));
    die("sendto()");
  }

  return true;
} // udp::send()

//=====================================
bool
send(int fd, const Contact &dest, sp::Buffer &buf) noexcept {
  ::sockaddr_in d;
  to_sockaddr(dest, d);
  return send(fd, d, buf);
} // udp::send()

bool
send(fd &fd, const Contact &dest, sp::Buffer &buf) noexcept {
  return send(int(fd), dest, buf);
} // udp::send()

//=====================================
} // namespace udp
