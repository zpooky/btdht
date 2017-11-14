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

/*
 * #Assumptions
 * - monothonic clock
 *
 * #Decisions based on stuff
 * -TODO nodeid+ip+port what denotes a duplicate contact?
 */

// TODO getopt: listen(port,ip) hex nodeid, repeating bootstrap nodes

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

template <typename Handle, typename Awake>
static void
loop(fd &fdpoll, Handle handle, Awake on_awake) noexcept {
  for (;;) {
    sp::byte in[2048];
    sp::byte out[2048];

    constexpr std::size_t max_events = 1024;
    ::epoll_event events[max_events];

    Timeout timeout = -1;
    int no_events = ::epoll_wait(int(fdpoll), events, max_events, timeout);
    time_t now = time(0);
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
          handle(from, inBuffer, outBuffer, now);
          flip(outBuffer);

          if (outBuffer.length > 0) {
            udp::send(cfd, /*to*/ from, outBuffer);
          }
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
    sp::Buffer outBuffer(out);
    timeout = on_awake(outBuffer, now);
  } // for
}

static dht::Module &
module_for(dht::Modules &modules, const char *key,
           dht::Module &error) noexcept {
  for (std::size_t i = 0; i < modules.length; ++i) {
    dht::Module &current = modules.module[i];
    if (std::strcmp(current.query, key) == 0) {
      return current;
    }
  }
  return error;
}

static bool
parse(dht::DHT &dht, dht::Modules &modules, const dht::Peer &peer,
      sp::Buffer &in, sp::Buffer &out, time_t now) noexcept {
  bencode::d::Decoder p(in);
  return bencode::d::dict(p, [&dht, &modules, &peer, &out, &now](auto &p) { //
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

    if (!(t && y && q)) {
      return false;
    }
    dht::Module error;
    error::setup(error);

    dht::MessageContext ctx{dht, p, out, transaction, peer, now};
    if (std::strcmp(message_type, "q") == 0) {
      /*query*/
      if (!bencode::d::value(p, "a")) {
        return false;
      }
      dht::Module &m = module_for(modules, message_type, error);
      return m.request(ctx);
    } else if (std::strcmp(message_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(p, "r")) {
        return false;
      }
      dht::Module &m = module_for(modules, message_type, error);
      return m.response(ctx);
    } else {
      return false;
    }
    return true;
  });
}

static void
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  ping::setup(modules.module[i++]);
  find_node::setup(modules.module[i++]);
  get_peers::setup(modules.module[i++]);
  announce_peer::setup(modules.module[i++]);
  error::setup(modules.module[i++]);
}

// echo "asd" | netcat --udp 127.0.0.1 45058
int
main() {
  dht::DHT dht;
  dht::randomize(dht.id);

  fd udp = udp::bind(INADDR_ANY, 0);
  dht::Peer bs_node(INADDR_ANY, udp::port(udp));

  printf("bind(%u)\n", udp::port(udp));

  fd poll = setup_epoll(udp);
  bootstrap(udp, dht.id, bs_node);

  dht::Modules modules;
  setup(modules);

  auto handle = [&modules, &dht](dht::Peer from, sp::Buffer &in,
                                 sp::Buffer &out, time_t now) {
    return parse(dht, modules, from, in, out, now);
  };

  auto awake = [&udp, &modules, &dht](sp::Buffer &out, time_t now) {
    return modules.on_awake(dht, udp, out, now);
  };

  loop(poll, handle, awake);
  return 0;
}
