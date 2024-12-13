#ifndef SP_MAINLINE_DHT_UPNP_H
#define SP_MAINLINE_DHT_UPNP_H

#include "util.h"

namespace upnp {
struct upnp {
  const char *protocol;
  Port local;
  Port external;
  Ip ip;
  sp::Seconds timeout;

  explicit upnp(sp::Seconds) noexcept;
};

// bool
// http_add_port(fd &, const upnp &) noexcept;
} // namespace upnp

#endif
