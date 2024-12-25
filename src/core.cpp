#include "core.h"

#include <errno.h>
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <sys/epoll.h> //epoll
#include <unistd.h>

namespace sp {
static void
die(const char *s) {
  fprintf(stderr, "%s", s);
  std::exit(1);
}

core::core() noexcept
    : epoll_fd{-1} {
  const int flag = 0;
  if ((epoll_fd = ::epoll_create1(flag)) < 0) {
    die("epoll_create1\n");
  }
}

core::~core() noexcept {
  if (epoll_fd >= 0) {
    close(epoll_fd);
    epoll_fd = -1;
  }
}

static int
tick(::epoll_event &current) {
  auto cb = (core_callback *)current.data.ptr;
  cb->callback(cb->closure, current.events);

  if (current.events & EPOLLIN) {
  } else if (current.events & EPOLLERR) {
    fprintf(stderr, "EPOLLERR\n");
  } else if (current.events & EPOLLHUP) {
    fprintf(stderr, "EPOLLHUP\n");
  } else if (current.events & EPOLLOUT) {
    fprintf(stderr, "EPOLLOUT\n");
  } else if (current.events & EPOLLRDHUP) {
    fprintf(stderr, "EPOLLRDHUP\n");
  }

  return 0;
}

int
core_tick(core &self, Milliseconds timeout) {
  int n_events;
  constexpr std::size_t max_events = 64;
  ::epoll_event events[max_events]{};

  n_events = ::epoll_wait(int(self.epoll_fd), events, max_events, int(timeout));
  if (n_events < 0) {
    if (errno == EAGAIN || errno == EINTR) {
    } else {
      die("epoll_wait");
    }
  }

  // fprintf(stdout, "============================\n");
  for (int i = 0; i < n_events; ++i) {
    tick(events[i]);
  } // for
  return 0;
}
} // namespace sp
