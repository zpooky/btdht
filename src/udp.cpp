#include "udp.h"
#include "bencode_print.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <util/assert.h>
// #include <exception>
#include "util.h"
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket
#include <sys/un.h>
#include <unistd.h> //close

namespace udp {
//=====================================
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
  sockaddr_in addr{};
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getsockname(int(listen), saddr, &slen);
  if (ret < 0) {
    return false;
  }

  return to_contact(addr, out);
}

//=====================================
fd
bind(Ipv4 ip, Port port, Mode mode) noexcept {
  int type = SOCK_DGRAM;
  if (mode == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd udp{::socket(AF_INET, type, IPPROTO_UDP)};
  if (!udp) {
    return udp;
  }

  ::sockaddr_in me{};
  std::memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(ip);
  ::sockaddr *meaddr = (::sockaddr *)&me;

  int ret = ::bind(int(udp), meaddr, sizeof(me));
  if (ret < 0) {
    return fd{-1};
  }

  return udp;
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
bind_unix(const char *file, Mode m) noexcept {
  int type = SOCK_DGRAM;
  if (m == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd udp{::socket(AF_UNIX, type, 0)};
  if (!udp) {
    return udp;
  }

  ::sockaddr_un name{};
  name.sun_family = AF_UNIX;
  strncpy(name.sun_path, file, strlen(file));

  if (::bind(int(udp), (struct sockaddr *)&name, sizeof(name)) < 0) {
    printf("2: %m\n");
    return fd{-1};
  }

 #if 0
  /* Prepare for accepting connections. The backlog size is set to 20. So while
   * one request is being processed other requests can be waiting. */
  if (::listen(int(udp), 20) < 0) {
    printf("3: %m\n");
    return fd{-1};
  }
#endif

  return udp;
}

fd
bind(Ipv6 ip, Port port, Mode mode) noexcept {
  int type = SOCK_DGRAM;
  int ret;
  if (mode == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd udp(::socket(AF_INET6, type, IPPROTO_UDP));
  if (!udp) {
    return udp;
  }

  int one = 1;
  ret = ::setsockopt(int(udp), IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
  if (ret < 0) {
    return fd{-1};
  }

  ::sockaddr_in6 me{};
  std::memset(&me, 0, sizeof(me));
  me.sin6_family = AF_INET6;
  me.sin6_port = htons(port);
  memcpy(&me.sin6_addr, ip.raw, sizeof(ip.raw));
  ::sockaddr *meaddr = (::sockaddr *)&me;

  ret = ::bind(int(udp), meaddr, sizeof(me));
  if (ret < 0) {
    return fd{-1};
  }

  return udp;
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

  if (len < 0) {
    assertxs(err != 0, err, strerror(err));
    return -err;
  }

  buf.pos += (size_t)len;
  return 0;
} // udp::receive()

int
receive(int fd, /*OUT*/ Contact &other, sp::Buffer &buf) noexcept {
  sp::BytesViewMark mrk = mark(buf);
  ::sockaddr_in remote{};
  memset(&remote, 0, sizeof(remote));

  int res = receive(fd, remote, buf);
  if (res == 0) {
    assertx_n(to_contact(remote, other));
    if (other.port == 0) {
      mrk.rollback = true;
      fprintf(stderr, "sockaddr_in[%s]", to_string(remote));
      char tmp[64]{0};
      to_string(other, tmp);
      fprintf(stderr, "Contact[%s]\n", tmp);
      // assertxs(other.port != 0, other.port, remote.sin_port);
      res = -EIO;
    }
  }

  return res;
} // udp::receive()

int
receive(fd &udp, /*OUT*/ Contact &c, /*OUT*/ sp::Buffer &b) noexcept {
  return receive(int(udp), c, b);
}

//=====================================
static bool
send(int fd, ::sockaddr_in &dest, const Contact &debug_dest,
     sp::Buffer &buf) noexcept {
  assertx(buf.length > 0);
  int flag = 0;
  ::sockaddr *destaddr = (::sockaddr *)&dest;

  int error = 0;
  ssize_t sent = 0;
  do {
    // sp::bencode_print(buf);
    sp::byte *const raw = offset(buf);
    const std::size_t raw_len = remaining_read(buf);

    if (raw_len > 0) {
      sent = ::sendto(fd, raw, raw_len, flag, destaddr, sizeof(dest));
      error = errno;
      if (sent > 0) {
        buf.pos += (size_t)sent;
      }
    }
  } while (sent < 0 && error == EAGAIN);

  if (sent < 0) {
    const std::size_t raw_len = remaining_read(buf);
    printf("sent[%zd] = "
           "sendto(fd[%d],raw,raw_len[%zu],flag[%d]),"
           "debug_dest[%s]): %s\n", //
           sent,                    //
           int(fd), raw_len, flag,  //
           to_string(debug_dest), strerror(error));
    assertx(false);
    return false;
  }

  return true;
} // udp::send()

//=====================================
bool
send(int fd, const Contact &dest, sp::Buffer &buf) noexcept {
  assertx(dest.port != 0);
  ::sockaddr_in d{};
  to_sockaddr(dest, d);
  return send(fd, d, dest, buf);
} // udp::send()

bool
send(fd &fd, const Contact &dest, sp::Buffer &buf) noexcept {
  return send(int(fd), dest, buf);
} // udp::send()

//=====================================
} // namespace udp
