#ifndef SP_MAINLINE_DHT_UDP_H
#define SP_MAINLINE_DHT_UDP_H

#include "util.h"

namespace udp {
//=====================================
bool
local(fd &, Contact &) noexcept;

//=====================================
enum class Mode { BLOCKING, NONBLOCKING };

fd bind(Ipv4, Port, Mode) noexcept;

fd bind_v4(Port, Mode) noexcept;

fd bind_v4(Mode) noexcept;

fd bind(Ipv6, Port, Mode) noexcept;

fd bind_v6(Port, Mode) noexcept;

fd bind_v6(Mode) noexcept;

//=====================================
int
receive(int fd, /*OUT*/ Contact &, /*OUT*/ sp::Buffer &) noexcept;

int
receive(fd &, /*OUT*/ Contact &, /*OUT*/ sp::Buffer &) noexcept;

//=====================================
bool
send(int fd, const Contact &, /*OUT*/ sp::Buffer &) noexcept;

bool
send(fd &, const Contact &, /*OUT*/ sp::Buffer &) noexcept;

//=====================================
} // namespace udp

#endif
