#include "dht.h"
#include "udp.h"
#include <stdio.h>

#include "BEncode.h"
#include "krpc.h"
#include "module.h"
#include "shared.h"
#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <sys/epoll.h> //epoll

static bool
random(krpc::Transaction &t) noexcept {
  const char *a = "aa";
  std::memcpy(t.id, a, 3);
  return true;
}

template <std::size_t capacity>
struct Modules {
  dht::Module module[capacity];
  std::size_t length;
  Modules()
      : module{}
      , length(0) {
  }
};

static void
die(const char *s) {
  perror(s);
  std::terminate();
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

static bool
bootstrap(fd &udp, const dht::NodeId &self, const dht::Peer &dest) noexcept {
  sp::byte rawb[1024];
  sp::Buffer buf(rawb);
  krpc::Transaction t;
  if (!krpc::request::find_node(buf, t, self, self)) {
    return false;
  }

  flip(buf);
  return udp::send(int(udp), dest, buf);
}

static int
on_timeout(dht::DHT &ctx) noexcept {
  // TODO
  return -1;
}

template <typename Handle>
static void
loop(fd &fdpoll, dht::DHT &ctx, Handle handle) noexcept {
  for (;;) {
    sp::byte in[2048];
    sp::byte out[2048];

    constexpr std::size_t max_events = 1024;
    ::epoll_event events[max_events];

    int timeout = -1;
    int no_events = ::epoll_wait(int(fdpoll), events, max_events, timeout);
    if (no_events < 0) {
      die("epoll_wait");
    }

    for (int i = 0; i < no_events; ++i) {
      ::epoll_event &current = events[i];
      if (current.events & EPOLLIN) {
        sp::Buffer inBuffer(in);
        sp::Buffer outBuffer(out);

        int cfd = current.data.fd;
        dht::Peer from;
        udp::receive(cfd, from, inBuffer);
        flip(inBuffer);

        if (inBuffer.length > 0) {
          dht::Peer remote;
          handle(ctx, from, inBuffer, outBuffer);
          flip(outBuffer);

          udp::send(cfd, /*to*/ from, outBuffer);
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
    timeout = on_timeout(ctx);
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
    krpc::Transaction transaction;
    char message_type[16] = {0};
    char query[16] = {0};
    bool t = false;
    bool y = false;
    bool q = false;

  start:
    if (!t && bencode::d::pair(p, "t", transaction.id)) {
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

    bool is_query = false;
    dht::Module error;
    error::setup(error);
    if (!(t && y && q)) {
      if (y) {
        is_query = std::strcmp(message_type, "q") == 0;
      }
      if (is_query) {
        // only send reply to a query
        return error.request(ctx, peer, p, out);
      }
      return false;
    }
    std::memcpy(p.transaction.id, transaction.id, sizeof(transaction.id));

    if (is_query) {
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
  error::setup(modules.module[i++]);

  parse(ctx, modules, peer, in, out);
}

// echo "asd" | netcat --udp 127.0.0.1 45058
int
main() {
  dht::DHT ctx;
  dht::randomize(ctx.id);

  fd udp = udp::bind(INADDR_ANY, 0);
  dht::Peer bs_node(INADDR_ANY, udp::port(udp));

  printf("bind(%u)\n", udp::port(udp));

  fd poll = setup_epoll(udp);
  bootstrap(udp, ctx.id, bs_node);
  loop(poll, ctx, handle);
  return 0;
}
