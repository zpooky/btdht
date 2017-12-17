#include "dht.h"
#include "udp.h"
#include <stdio.h>

#include "bencode.h"
#include "krpc.h"
#include "module.h"
#include "shared.h"
#include <algorithm>
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
bootstrap(dht::DHT &dht, dht::Contact dest, time_t now) noexcept {
  // placeholder 000 id
  dht::NodeId id;
  dht::Node contact(id, dest, now);
  return dht::insert(dht, contact) != nullptr;
}

template <typename Handle, typename Awake>
static void
loop(fd &fdpoll, Handle handle, Awake on_awake) noexcept {
  time_t previous = 0;
  for (;;) {
    sp::byte in[2048];
    sp::byte out[2048];

    constexpr std::size_t max_events = 1024;
    ::epoll_event events[max_events];

    Timeout timeout = -1;
    int no_events = ::epoll_wait(int(fdpoll), events, max_events, timeout);
    if (no_events < 0) {
      die("epoll_wait");
    }

    // always increasing clock
    time_t now = std::max(time(0), previous);

    for (int i = 0; i < no_events; ++i) {

      ::epoll_event &current = events[i];
      if (current.events & EPOLLIN) {

        sp::Buffer inBuffer(in);
        sp::Buffer outBuffer(out);

        int cfd = current.data.fd;
        dht::Contact from;
        udp::receive(cfd, from, inBuffer);
        flip(inBuffer);

        if (inBuffer.length > 0) {
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
    timeout = on_awake(outBuffer);

    previous = now;
  } // for
}

static dht::Module &
module_for(dht::Modules &ms, const char *key, dht::Module &error) noexcept {
  for (std::size_t i = 0; i < ms.length; ++i) {
    dht::Module &current = ms.module[i];
    if (std::strcmp(current.query, key) == 0) {
      return current;
    }
  }
  return error;
}

static bool
parse(dht::DHT &dht, dht::Modules &modules, const dht::Contact &peer,
      sp::Buffer &in, sp::Buffer &out, time_t now) noexcept {

  auto f = //
      [&dht, &modules, &peer, &out, now](krpc::ParseContext &pctx) {
        dht::Module error;
        error::setup(error);

        dht::MessageContext ctx{dht, pctx, out, peer, now};
        if (std::strcmp(pctx.msg_type, "q") == 0) {
          /*query*/
          if (!bencode::d::value(pctx.decoder, "a")) {
            return false;
          }

          dht::Module &m = module_for(modules, pctx.query, error);
          return m.request(ctx);
        } else if (std::strcmp(pctx.msg_type, "r") == 0) {
          assert(pctx.query == nullptr);
          /*response*/
          if (!bencode::d::value(pctx.decoder, "r")) {
            return false;
          }

          dht::TxHandle handle = dht::take_tx(dht.client, pctx.tx);
          if (handle) {
            return handle(ctx);
          }
        }
        return false;
      };

  bencode::d::Decoder d(in);
  krpc::ParseContext pctx(d);
  if (!krpc::d::krpc(pctx, f)) {
    return false;
  }
  return true;
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
  fd udp = udp::bind(INADDR_ANY, 0);

  dht::DHT dht(udp);
  if (!dht::init(dht)) {
    die("failed to init dht");
  }
  dht::Contact bs_node(INADDR_ANY, udp::port(udp)); // TODO

  printf("bind(%u)\n", udp::port(udp));

  fd poll = setup_epoll(udp);

  time_t tnow = time(nullptr);
  bootstrap(dht, bs_node, tnow);

  dht::Modules modules;
  setup(modules);

  auto handle = //
      [&](dht::Contact from, sp::Buffer &in, sp::Buffer &out, time_t now) {
        // TODO set last activit after, and how to handle first time
        dht.last_activity = now;
        dht.now = now;

        return parse(dht, modules, from, in, out, now);
      };

  auto awake = [&modules, &dht](sp::Buffer &out) {
    return modules.on_awake(dht, out);
  };

  loop(poll, handle, awake);
  return 0;
}
