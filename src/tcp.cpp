#include "tcp.h"
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket
#include <unistd.h>     //close

namespace tcp {
//=====================================
bool
local(fd &listen, Contact &out) noexcept {
  assertx(listen);

  sockaddr_in addr{};
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int res = ::getsockname(int(listen), saddr, &slen);
  if (res < 0) {
    return false;
  }

  return to_contact(addr, out);
}

//=====================================
bool
remote(fd &listen, Contact &out) noexcept {
  assertx(listen);

  sockaddr_in addr{};
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int res = ::getpeername(int(listen), saddr, &slen);
  if (res < 0) {
    return false;
  }

  return to_contact(addr, out);
}

//=====================================
fd
connect(const Contact &dest, Mode mode) noexcept {
  int type = SOCK_STREAM;
  if (mode == Mode::NONBLOCKING) {
    type |= SOCK_NONBLOCK;
  }

  fd tcp{::socket(AF_INET, type, 0)};
  if (!tcp) {
    return fd{-1};
  }

  ::sockaddr_in me{};
  std::memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_port = htons(0);

  ::sockaddr_in remote{};
  std::memset(&remote, 0, sizeof(remote));
  if (!to_sockaddr(dest, remote)) {
    return fd{-1};
  }

  int res = connect(int(tcp), (sockaddr *)&remote, sizeof(sockaddr));
  if (res < 0) {
    return fd{-1};
  }

  return tcp;
}

//=====================================
int
send(fd &self, /*OUT*/ sp::Buffer &b) noexcept {
  assertx(bool(self));
  int flags = 0;

  int error = 0;
  ssize_t sent = 0;
  do {
    std::size_t len = remaining_read(b);
    if (len > 0) {
      sent = ::send(int(self), offset(b), len, flags);
      error = errno;

      if (sent > 0) {
        b.pos += sent;
      }
    } else {
      break;
    }
  } while (sent < 0 && error == EAGAIN);

  return error;
}

//=====================================
int
read(fd &self, /*OUT*/ sp::Buffer &b) noexcept {
  assertx(bool(self));

  int error = 0;
  ssize_t read = 0;
  do {
    std::size_t len = remaining_write(b);
    if (len > 0) {
      read = ::read(int(self), offset(b), len);
      error = errno;

      if (read > 0) {
        b.pos += read;
      }
    } else {
      break;
    }
  } while (read < 0 && error == EAGAIN);

  return 0;
}

//=====================================
} // namespace tcp
