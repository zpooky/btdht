#include "udp.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
// #include <exception>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h> //close

#include <util/assert.h>

#include "bencode_print.h"
#include "util.h"

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
fd
bind(Ipv4 ip, Port port, Mode mode) noexcept {
  int type = SOCK_DGRAM | SOCK_CLOEXEC;
  if (mode == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd udp{::socket(AF_INET, type, IPPROTO_UDP)};
  if (!udp) {
    return udp;
  }

  ::sockaddr_in me{};
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

static fd
do_bind_unix(const char *file, Mode m, int type) noexcept {
  mode_t before;
  if (m == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd udp{::socket(AF_UNIX, type, 0)};
  if (!udp) {
    return udp;
  }

  unlink(file);

  before = ::umask(077);
  ::sockaddr_un name{};
  name.sun_family = AF_UNIX;
  strncpy(name.sun_path, file, strlen(file));

  int ret = ::bind(int(udp), (struct sockaddr *)&name, sizeof(name));
  ::umask(before);
  if (ret < 0) {
    fprintf(stderr, "%s: 2: %s\n", __func__, strerror(errno));
    return fd{-1};
  }

  return udp;
}

fd
bind_unix(const char *file, Mode m) noexcept {
  int type = SOCK_DGRAM;
  return do_bind_unix(file, m, type);
}

fd
bind_unix_seq(const char *file, Mode m) noexcept {
  int type = SOCK_SEQPACKET;
  fd seq_fd = do_bind_unix(file, m, type);
  if (bool(seq_fd)) {
    /* Prepare for accepting connections. The backlog size is set to 20. So
     * while one request is being processed other requests can be waiting. */
    if (::listen(int(seq_fd), 20) < 0) {
      fprintf(stderr, "%s: 3: %s\n", __func__, strerror(errno));
      return fd{-1};
    }
  }

  return seq_fd;
}

static fd
do_connect_unix(const char *file, Mode m, int type) noexcept {
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

  if (::connect(int(udp), (struct sockaddr *)&name, sizeof(name)) < 0) {
    fprintf(stderr, "%s: 2: %s\n", __func__, strerror(errno));
    return fd{-1};
  }

  return udp;
}

fd
connect_unix_seq(const char *file, Mode m) noexcept {
  int type = SOCK_SEQPACKET;
  return do_connect_unix(file, m, type);
}

fd
connect(Ipv4 ip, Port port, Mode m) noexcept {
  int type = SOCK_DGRAM | SOCK_CLOEXEC;
  if (m == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd udp{::socket(AF_INET, type, IPPROTO_UDP)};
  if (!udp) {
    fprintf(stderr, "socket: %s\n", strerror(errno));
    return udp;
  }

  /*For source address*/
#if 0
  ::addrinfo hints{0};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM; // UDP communication
  hints.ai_flags = AI_PASSIVE;    // fill in my IP for me
#endif

  ::sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_port = htons(0);
  local.sin_addr.s_addr = htonl(0);

  /*Bind this datagram socket to source address info */
  if (::bind(int(udp), (::sockaddr *)&local, sizeof(local)) < 0) {
    fprintf(stderr, "bind: %s\n", strerror(errno));
    return fd{-1};
  }

  ::sockaddr_in remote{};
  remote.sin_family = AF_INET;
  remote.sin_port = htons(uint16_t{port});
  remote.sin_addr.s_addr = htonl(ip);
  // ::sockaddr *meaddr = (::sockaddr *)&remote;

  if (::connect(int(udp), (::sockaddr *)&remote, sizeof(remote)) < 0) {
    fprintf(stderr, "%s: 2: %s\n", __func__, strerror(errno));
    return fd{-1};
  }

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
    fprintf(stderr,
            "sent[%zd] = "
            "sendto(fd[%d],raw,raw_len[%zu],flag[%d]),"
            "debug_dest[%s]): %s\n", //
            sent,                    //
            int(fd), raw_len, flag,  //
            to_string(debug_dest), strerror(error));
    assertxs(false, sent, fd, raw_len, flag, to_string(debug_dest),error, strerror(error));
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

} // namespace udp

namespace net {
//=====================================
bool
local(const fd &listen, Contact &out) noexcept {
  sockaddr_in addr{};
  socklen_t slen = sizeof(addr);

  int ret = ::getsockname(int(listen), (sockaddr *)&addr, &slen);
  if (ret < 0) {
    return false;
  }

  return to_contact(addr, out);
}

bool
remote(const fd &listen, Contact &out) noexcept {
  sockaddr_in addr{};
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getpeername(int(listen), saddr, &slen);
  if (ret < 0) {
    return false;
  }

  return to_contact(addr, out);
}

//=====================================
int
sock_read(fd &fd, sp::Buffer &buf) noexcept {
  sp::byte *const raw = offset(buf);
  std::size_t raw_len = remaining_write(buf);

  ssize_t len = 0;
  len = ::read(int(fd), raw, raw_len);
  int err = errno;

  if (len < 0) {
    assertxs(err != 0, err, strerror(err));
    return -err;
  }

  buf.pos += (size_t)len;
  return 0;
}

//=====================================
bool
sock_write(fd &fd, sp::Buffer &buf) noexcept {
  sp::byte *const raw = offset(buf);
  const std::size_t raw_len = remaining_read(buf);

  if (raw_len > 0) {
    ssize_t sent = write(int(fd), raw, raw_len);
    if (sent > 0) {
      buf.pos += (size_t)sent;
    } else {
      return false;
    }
  }

  return true;
}

//=====================================
} // namespace net
