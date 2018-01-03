#ifndef SP_MAINLINE_DHT_UDP_H
#define SP_MAINLINE_DHT_UDP_H

#include "shared.h"

namespace udp { //

Contact
local(fd &) noexcept;

fd
bind(Ipv4 ip, Port port) noexcept;

void
receive(int fd, Contact &, sp::Buffer &) noexcept;

bool
send(int fd, const Contact &, sp::Buffer &) noexcept;

bool
send(fd &, const Contact &, sp::Buffer &) noexcept;

} // namespace udp

#endif
