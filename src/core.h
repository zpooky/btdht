#ifndef SP_MAINLINE_DHT_CORE_H
#define SP_MAINLINE_DHT_CORE_H

#include <util/timeout.h>

namespace sp {
struct core {
  int epoll_fd;

  core() noexcept;
  virtual ~core() noexcept;
};

struct core_callback {
  void *closure;
  int (*callback)(void *closure, uint32_t events);
};

int
core_tick(core &self, Milliseconds timeout);

} // namespace sp

#endif
