#include "mainline.h"
#include <stdio.h>

#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <sys/epoll.h>  //epoll
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
  sockaddr_in me;
  std::memset(&me, 0, sizeof(me));

  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(INADDR_ANY);

  // bind socket to port
  if (bind(udp, (sockaddr *)&me, sizeof(me)) == -1) {
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

int
main() {
  dht::Peer bs_node;
  dht::DHT dht;
  dht::randomize(dht.id);

  Port listen(0);
  fd udp = bind(listen);
  fd poll = setup_epoll(udp);
  bootstrap(dht.id, bs_node);
  return 0;
}
