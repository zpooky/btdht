#include "dht.h"
#include <stdio.h>

#include "BEncode.h"
#include "krpc.h"
#include "module.h"
#include "shared.h"
#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <sys/epoll.h>  //epoll
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket

template <std::size_t capacity>
struct Modules {
  dht::Module module[capacity];
  std::size_t length;
  Modules()
      : module{}
      , length(0) {
  }
};

void
die(const char *s) {
  perror(s);
  std::terminate();
}

static Port
lport(fd &listen) noexcept {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getsockname(int(listen), saddr, &slen);
  if (ret < 0) {
    die("getsockname()");
  }
  return ntohs(addr.sin_port);
}

static fd
bind(Ip ip, Port port) {
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

static fd
setup_epoll(fd &udp) noexcept {
  const int flag = 0;
  const int poll = ::epoll_create1(flag);
  if (poll < 0) {
    die("epoll_create1");
  }

  ::epoll_event ev;
  // this are the events we are polling for
  // `man epoll_ctl` for list of events
  // EPOLLIN: ready for read()
  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
  ev.data.fd = int(udp);

  // add fd to poll on
  int res = ::epoll_ctl(poll, EPOLL_CTL_ADD, int(udp), &ev);
  if (res < 0) {
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
    len = ::recvfrom(fd, raw, raw_len, flag, /*OUT*/ o, &slen);
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
to_sockaddr(const dht::Peer &src, ::sockaddr_in &dest) noexcept {
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = htonl(src.ip);
  dest.sin_port = htons(src.port);
}

static void
to_peer(const ::sockaddr_in &src, dht::Peer &dest) noexcept {
  dest.ip = ntohl(src.sin_addr.s_addr);
  dest.port = ntohs(src.sin_port);
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

template <typename Handle>
static void
loop(fd &fdpoll, dht::DHT &ctx, Handle handle) noexcept {
  for (;;) {
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
        ::sockaddr_in from;
        udp_receive(cfd, from, inBuffer);
        flip(inBuffer);

        if (inBuffer.length > 0) {
          dht::Peer remote;
          to_peer(from, remote);
          handle(ctx, remote, inBuffer, outBuffer);
          flip(outBuffer);

          udp_send(cfd, /*to*/ from, outBuffer);
        }
      }
      if (current.events & EPOLLERR) {
        printf("EPOLLERR\n");
      }
      if (current.events & EPOLLHUP) {
        printf("EPOLLHUP\n");
      }
      if (current.events & EPOLLOUT) {
        printf("EPOLLOUT\n");
      }
    }
  } // for
}

template <std::size_t capacity>
static dht::Module &
module_for(Modules<capacity> &modules, const char *key,
           dht::Module &error) noexcept {
  for (std::size_t i = 0; i < modules.length; ++i) {
    dht::Module &current = modules.module[i];
    if (std::strcmp(current.query, key) == 0) {
      return current;
    }
  }
  return error;
}

template <std::size_t capacity>
static bool
parse(dht::DHT &ctx, Modules<capacity> &modules, const dht::Peer &peer,
      sp::Buffer &in, sp::Buffer &out) noexcept {
  bencode::d::Decoder p(in);
  return bencode::d::dict(p, [&ctx, &modules, &peer, &out](auto &p) { //
    sp::byte transaction[16] = {0};
    char message_type[16] = {0};
    char query[16] = {0};
    bool t = false;
    bool y = false;
    bool q = false;

  start:
    if (!t && bencode::d::pair(p, "t", transaction)) {
      t = true;
      goto start;
    }
    if (!y && bencode::d::pair(p, "y", message_type)) {
      y = true;
      goto start;
    }
    if (!q && bencode::d::pair(p, "q", query)) {
      q = true;
      goto start;
    }

    dht::Module error;
    error::setup(error);
    if (!(t && y && q)) {
      return error.request(ctx, peer, p, out);
    }

    if (std::strcmp(message_type, "q") == 0) {
      /*query*/
      if (!bencode::d::value(p, "a")) {
        return false;
      }
      dht::Module &m = module_for(modules, message_type, error);
      return m.request(ctx, peer, p, out);
    } else if (std::strcmp(message_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(p, "r")) {
        return false;
      }
      dht::Module &m = module_for(modules, message_type, error);
      return m.request(ctx, peer, p, out);
    } else {
      return false;
    }
    return true;
  });
}

static void
handle(dht::DHT &ctx, const dht::Peer &peer, sp::Buffer &in,
       sp::Buffer &out) noexcept {
  Modules<4> modules;
  std::size_t &i = modules.length;
  ping::setup(modules.module[i++]);
  find_node::setup(modules.module[i++]);
  get_peers::setup(modules.module[i++]);
  announce_peer::setup(modules.module[i++]);

  parse(ctx, modules, peer, in, out);
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

// echo "asd" | netcat --udp 127.0.0.1 45058
int
main() {
  dht::DHT ctx;
  dht::randomize(ctx.id);

  fd udp = bind(INADDR_ANY, 0);
  dht::Peer bs_node(INADDR_ANY, lport(udp));

  printf("bind(%u)\n", lport(udp));

  fd poll = setup_epoll(udp);
  bootstrap(udp, ctx.id, bs_node);
  loop(poll, ctx, handle);
  return 0;
}
