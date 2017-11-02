#include "mainline.h"
#include <stdio.h>

#include "shared.h"
#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <sys/epoll.h>  //epoll
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket
#include <unistd.h>     //close

class fd {
private:
  int m_fd;

public:
  explicit fd(int p_fd)
      : m_fd(p_fd) {
  }

  fd(const fd &) = delete;
  fd(fd &&o)
      : m_fd(o.m_fd) {
    o.m_fd = -1;
  }

  ~fd() {
    if (m_fd > 0) {
      ::close(m_fd);
    }
  }

  explicit operator int() noexcept {
    return m_fd;
  }
};

using Port = std::uint16_t;

void
die(const char *s) {
  perror(s);
  std::terminate();
}

static fd
bind(Port port) {
  int udp =
      ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
  if (udp < 0) {
    die("socket()\n");
  }
  ::sockaddr_in me;
  std::memset(&me, 0, sizeof(me));

  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(udp, (::sockaddr *)&me, sizeof(me)) == -1) {
    die("bind");
  }
  return fd{udp};
}

static fd
setup_epoll(fd &udp) noexcept {
  int poll = ::epoll_create1(0);
  if (poll < 0) {
    die("epoll_create1");
  }

  ::epoll_event ev;

  ev.events = EPOLLIN;
  ev.data.fd = int(udp);
  if (::epoll_ctl(poll, EPOLL_CTL_ADD, int(udp), &ev) < 0) {
    die("epoll_ctl: listen_sock");
  }

  return fd{poll};
}

static void
bootstrap(const dht::NodeId &self, const dht::Peer &) noexcept {
}

static void
udp_receive(int fd, ::sockaddr_in &other, sp::Buffer &buf) noexcept {
  int flag = 0;
  sockaddr *o = (sockaddr *)&other;
  socklen_t slen = sizeof(other);

  sp::byte *const raw = offset(buf);
  const std::size_t raw_len = remaining_write(buf);

  ssize_t len = 0;
  do {
    len = ::recvfrom(fd, raw, raw_len, flag, o, &slen);
  } while (len < 0 && errno == EAGAIN);

  if (len < 0) {
    die("recvfrom()");
  }
  buf.pos += len;
}

static void
udp_send(int fd, sp::Buffer) noexcept {
}

static void
loop(fd &fdpoll, dht::DHT &ctx,
     void (*f)(dht::DHT &, sp::Buffer &, sp::Buffer &)) noexcept {
  sp::byte in[2048];
  sp::byte out[2048];

  constexpr std::size_t max_events = 1024;
  ::epoll_event events[max_events];

  int no_events = ::epoll_wait(int(fdpoll), events, max_events, -1);
  if (no_events < 0) {
    die("epoll_wait");
  }

  for (int i = 0; i < no_events; ++i) {

    ::epoll_event &current = events[i];
    if (current.events & EPOLLIN) {
      sp::Buffer inBuffer(in);
      sp::Buffer outBuffer(out);

      int cfd = current.data.fd;
      ::sockaddr_in other;
      udp_receive(cfd, other, inBuffer);
      flip(inBuffer);

      if (inBuffer.length > 0) {
        f(ctx, inBuffer, outBuffer);
        flip(outBuffer);

        udp_send(cfd, outBuffer);
      }
    }
  }
}

static void
handle(dht::DHT &ctx, sp::Buffer &in, sp::Buffer &out) noexcept {
}

int
main() {
  dht::Peer bs_node;
  dht::DHT dht;
  dht::randomize(dht.id);

  Port listen(0);
  fd udp = bind(listen);
  fd poll = setup_epoll(udp);
  bootstrap(dht.id, bs_node);
  loop(poll, dht, handle);
  return 0;
}
