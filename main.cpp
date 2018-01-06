#include "dht.h"
#include "udp.h"
#include <stdio.h>

#include "Log.h"
#include "Options.h"
#include "bencode.h"
#include "krpc.h"
#include "module.h"
#include "shared.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
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
bootstrap(dht::DHT &dht, Contact dest) noexcept {
  return sp::push_back(dht.bootstrap_contacts, dest);
}

template <typename Handle, typename Awake>
static void
loop(fd &fdpoll, Handle handle, Awake on_awake) noexcept {
  time_t previous = 0;

  sp::byte in[2048];
  sp::byte out[2048];

  Timeout timeout = 0;
  for (;;) {

    constexpr std::size_t max_events = 1024;
    ::epoll_event events[max_events];

    int no_events = ::epoll_wait(int(fdpoll), events, max_events, timeout);
    if (no_events < 0) {
      if (errno != EINTR) {
        // TODO handle specific interrupt
        die("epoll_wait");
      }
    }

    // always increasing clock
    time_t now = std::max(time(nullptr), previous);

    for (int i = 0; i < no_events; ++i) {

      ::epoll_event &current = events[i];
      if (current.events & EPOLLIN) {

        sp::Buffer inBuffer(in);
        sp::Buffer outBuffer(out);

        int cfd = current.data.fd;
        Contact from;
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
    timeout = on_awake(outBuffer, now);

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
parse(dht::DHT &dht, dht::Modules &modules, const Contact &peer, sp::Buffer &in,
      sp::Buffer &out) noexcept {

  auto f = [&dht, &modules, &peer, &out](krpc::ParseContext &pctx) {
    dht::Module error;
    error::setup(error);

    dht::MessageContext ctx{dht, pctx, out, peer};
    if (std::strcmp(pctx.msg_type, "q") == 0) {
      /*query*/
      if (!bencode::d::value(pctx.decoder, "a")) {
        return false;
      }

      dht::Module &m = module_for(modules, pctx.query, error);
      return m.request(ctx);
    } else if (std::strcmp(pctx.msg_type, "r") == 0) {
      /*response*/
      if (!bencode::d::value(pctx.decoder, "r")) {
        return false;
      }

      dht::TxContext context;
      if (dht::take_tx(dht.client, pctx.tx, context)) {
        log::receive::res::known_tx(ctx);
        return context.handle(ctx);
      } else {
        log::receive::res::unknown_tx(ctx);
        assert(false);
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

template <typename T, std::size_t SIZE, typename F>
static void
for_each(T (&arr)[SIZE], F f) noexcept {
  for (std::size_t i = 0; i < SIZE; ++i) {
    f(arr[i]);
  }
}

// transmission-daemon -er--dht
// echo "asd" | netcat --udp 127.0.0.1 45058
int
main(int argc, char **argv) {
  dht::Options options;
  // if (!dht::parse(argc, argv, options)) {
  //   die("TODO");
  // }
  fd udp = udp::bind(INADDR_ANY, 42605);
  // fd udp = udp::bind(INADDR_ANY, 0);
  Contact local = udp::local(udp);

  dht::DHT dht(udp, local);
  if (!dht::init(dht)) {
    die("failed to init dht");
  }
  char str[256] = {0};
  assert(to_string(local, str, sizeof(str)));
  printf("bind(%s)\n", str);

  /*boostrap*/
  // Contact bs_node(INADDR_ANY, local.port); // TODO
  const char *bss[] = {
      "192.168.1.47:13596", "127.0.0.1:13596", "213.65.130.80:13596", //
      "192.168.1.47:51413", "127.0.0.1:51413", "213.65.130.80:51413", //
  };
  dht.now = time(nullptr);
  for_each(bss, [&dht](const char *ip) {
    Contact bs(0, 0);
    if (!convert(ip, bs)) {
      die("parse bootstrap ip failed");
    }

    assert(bs.ipv4 > 0);
    assert(bs.port > 0);

    Contact node(bs);
    if (!bootstrap(dht, node)) {
      die("failed to setup bootstrap");
    }
  });

  fd poll = setup_epoll(udp);

  dht::Modules modules;
  setup(modules);

  auto handle = //
      [&](Contact from, sp::Buffer &in, sp::Buffer &out, time_t now) {
        dht.last_activity = dht.last_activity == 0 ? now : dht.last_activity;
        dht.now = now;

        const sp::Buffer copy(in);
        if (!parse(dht, modules, from, in, out)) {
          log::receive::parse::error(dht, copy);
          return false;
        }
        return true;
      };

  auto awake = [&modules, &dht](sp::Buffer &out, time_t now) {
    auto result = modules.on_awake(dht, out);
    log::awake::timeout(dht, result);
    dht.last_activity = now;
    return result;
  };

  loop(poll, handle, awake);
  return 0;
}
