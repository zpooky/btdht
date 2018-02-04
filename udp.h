#ifndef SP_MAINLINE_DHT_UDP_H
#define SP_MAINLINE_DHT_UDP_H

#include "shared.h"

namespace udp { //

Contact
local(fd &) noexcept;

enum class Mode { BLOCKING, NONBLOCKING };

fd bind(Ipv4, Port, Mode) noexcept;

fd bind(Port, Mode) noexcept;

void
receive(int fd, /*OUT*/ Contact &, /*OUT*/ sp::Buffer &) noexcept;

void
receive(fd &, /*OUT*/ Contact &, /*OUT*/ sp::Buffer &) noexcept;

bool
send(int fd, const Contact &, /*OUT*/ sp::Buffer &) noexcept;

bool
send(fd &, const Contact &, /*OUT*/ sp::Buffer &) noexcept;

} // namespace udp

#endif
