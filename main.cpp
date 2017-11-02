#include "mainline.h"
#include <stdio.h>

#include "krpc.h"
#include "shared.h"
#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <sys/epoll.h>  //epoll
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket

void
die(const char *s) {
  perror(s);
  std::terminate();
}

static Port
listen_port(fd &listen) noexcept {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getsockname(int(listen), saddr, &slen);
  if (ret < 0) {
    die("getsockname()");
  }
  return addr.sin_port;
}

static fd
bind(Port port) {
  int udp =
      ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
  if (udp < 0) {
    die("socket()");
  }
  ::sockaddr_in me;
  std::memset(&me, 0, sizeof(me));
  ::sockaddr *meaddr = (::sockaddr *)&me;

  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(udp, meaddr, sizeof(me)) == -1) {
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

static bool
udp_send(int fd, ::sockaddr_in &dest, sp::Buffer &buf) noexcept {
  int flag = 0;
  sockaddr *destaddr = (sockaddr *)&dest;

  sp::byte *const raw = offset(buf);
  const std::size_t raw_len = remaining_read(buf);

  ssize_t sent = 0;
  do {
    sent = ::sendto(fd, raw, raw_len, flag, destaddr, sizeof(dest));
  } while (sent < 0 && errno == EAGAIN);

  if (sent < 0) {
    die("recvfrom()");
  }
  buf.pos += sent;

  return true;
}

static void
to_sockaddr(const dht::Peer &p, ::sockaddr_in &dest) noexcept {
}

static bool
bootstrap(fd &udp, const dht::NodeId &self, const dht::Peer &target) noexcept {
  sp::byte rawb[1024];
  sp::Buffer buf(rawb);
  if (!krpc::request::find_node(buf, self, self)) {
    return false;
  }

  ::sockaddr_in dest;
  to_sockaddr(target, dest);

  flip(buf);
  return udp_send(int(udp), dest, buf);
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
      ::sockaddr_in remote;
      udp_receive(cfd, remote, inBuffer);
      flip(inBuffer);

      if (inBuffer.length > 0) {
        f(ctx, inBuffer, outBuffer);
        flip(outBuffer);

        udp_send(cfd, remote, outBuffer);
      }
    }
  }
}

static void
handle(dht::DHT &ctx, sp::Buffer &in, sp::Buffer &out) noexcept {
}

int
main() {
  dht::DHT dht;
  dht::randomize(dht.id);

  fd udp = bind(0);
  dht::Peer bs_node(INADDR_LOOPBACK, listen_port(udp));
  printf("bind(%u)\n", bs_node.port);

  fd poll = setup_epoll(udp);
  bootstrap(udp, dht.id, bs_node);
  loop(poll, dht, handle);
  return 0;
}
