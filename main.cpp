#include "mainline.h"
#include <stdio.h>

#include "BEncode.h"
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
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = htonl(p.ip);
  dest.sin_port = htons(p.port);
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
        ::sockaddr_in remote;
        udp_receive(cfd, remote, inBuffer);
        flip(inBuffer);

        if (inBuffer.length > 0) {
          f(ctx, inBuffer, outBuffer);
          flip(outBuffer);

          udp_send(cfd, remote, outBuffer);
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

struct dht_impls { //
  void (*req_ping)(dht::DHT &, sp::Buffer &, const dht::NodeId &);
  void (*req_find_node)(dht::DHT &, sp::Buffer &, //
                        const dht::NodeId &, const dht::NodeId &);
  void (*req_get_peers)(dht::DHT &, sp::Buffer &, //
                        const dht::NodeId &, const dht::Infohash &);
  void (*req_announce)(dht::DHT &, sp::Buffer &, //
                       const dht::NodeId &, bool, const dht::Infohash &, Port,
                       const char *);

  void (*res_ping)(dht::DHT &, sp::Buffer &);
  void (*res_find_node)(dht::DHT &, sp::Buffer &);
  void (*res_get_peers)(dht::DHT &, sp::Buffer &);
  void (*res_announce)(dht::DHT &, sp::Buffer &);
};

static bool
parse(dht::DHT &ctx, sp::Buffer &in, sp::Buffer &out,
      dht_impls &impl) noexcept {
  bencode::d::Decoder p(in);
  return bencode::d::dict(p, [&ctx, &out, &impl](auto &p) { //
    sp::byte transaction[16] = {0};
    char messageType[2] = {0};
    char query[16] = {0};
    bool t = false;
    bool y = false;
    bool q = false;

  start:
    if (!t && bencode::d::pair(p, "t", transaction)) {
      t = true;
      goto start;
    }
    if (!y && bencode::d::pair(p, "y", messageType)) {
      y = true;
      goto start;
    }
    if (!q && bencode::d::pair(p, "q", query)) {
      q = true;
      goto start;
    }

    if (!(t && y && q)) {
      return false;
    }

    if (strcmp(messageType, "q") == 0) {
      if (!bencode::d::value(p, "a")) {
        return false;
      }

      // TODO support out of order
      /*query*/
      if (strcmp(query, "ping") == 0) {
        return bencode::d::dict(p, [&ctx, &out, &impl](auto &p) { //
          dht::NodeId id;
          if (!bencode::d::pair(p, "id", id.id)) {
            return false;
          }

          impl.req_ping(ctx, out, id);
          return true;
        });
      } else if (strcmp(query, "find_node") == 0) {
        return bencode::d::dict(p, [&ctx, &out, &impl](auto &p) { //
          dht::NodeId id;
          dht::NodeId target;

          if (!bencode::d::pair(p, "id", id.id)) {
            return false;
          }
          if (!bencode::d::pair(p, "target", target.id)) {
            return false;
          }

          impl.req_find_node(ctx, out, id, target);
          return true;
        });
      } else if (strcmp(query, "get_peers") == 0) {
        return bencode::d::dict(p, [&ctx, &out, &impl](auto &p) {
          dht::NodeId id;
          dht::Infohash infohash;

          if (!bencode::d::pair(p, "id", id.id)) {
            return false;
          }
          if (!bencode::d::pair(p, "info_hash", infohash.id)) {
            return false;
          }

          impl.req_get_peers(ctx, out, id, infohash);
          return true;
        });
      } else if (strcmp(query, "announce_peer") == 0) {
        return bencode::d::dict(p, [&ctx, &out, &impl](auto &p) {
          dht::NodeId id;
          bool implied_port = false;
          dht::Infohash infohash;
          Port port = 0;
          char token[16] = {0};

          if (!bencode::d::pair(p, "id", id.id)) {
            return false;
          }
          if (!bencode::d::pair(p, "implied_port", implied_port)) {
            return false;
          }
          if (!bencode::d::pair(p, "info_hash", infohash.id)) {
            return false;
          }
          // TODO optional
          if (!bencode::d::pair(p, "port", port)) {
            return false;
          }
          if (!bencode::d::pair(p, "token", token)) {
            return false;
          }

          impl.req_announce(ctx, out, id, implied_port, infohash, port, token);
          return true;
        });
      } else {
        return false;
      }
    } else if (strcmp(messageType, "r") == 0) {
      /*response*/
      // TODO
    } else {
      return false;
    }
    return true;
  });
}

static void
handle_Ping(dht::DHT &ctx, sp::Buffer &out, //
            const dht::NodeId &sender) noexcept {
}

static void
handle_FindNode(dht::DHT &ctx, sp::Buffer &out, //
                const dht::NodeId &self, const dht::NodeId &search) noexcept {
}

static void
handle_GetPeers(dht::DHT &ctx, sp::Buffer &out, //
                const dht::NodeId &id, const dht::Infohash &infohash) noexcept {
}

static void
handle_Announce(dht::DHT &ctx, sp::Buffer &out, //
                const dht::NodeId &id, bool implied_port,
                const dht::Infohash &infohash, Port port,
                const char *token) noexcept {
}

static void
handle(dht::DHT &ctx, sp::Buffer &in, sp::Buffer &out) noexcept {
  dht_impls impls;
  impls.req_ping = handle_Ping;
  impls.req_find_node = handle_FindNode;
  impls.req_get_peers = handle_GetPeers;
  impls.req_announce = handle_Announce;

  parse(ctx, in, out, impls);
  printf("handle\n");
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
