#ifndef SP_MAINLINE_DHT_UDP_H
#define SP_MAINLINE_DHT_UDP_H

#include "shared.h"

namespace udp { //

Port
port(fd &listen) noexcept;

fd
bind(Ipv4 ip, Port port) noexcept;

void
receive(int fd, dht::Contact &other, sp::Buffer &buf) noexcept;

bool
send(int fd, const dht::Contact &dest, sp::Buffer &buf) noexcept;

bool
send(fd &, const dht::Contact &dest, sp::Buffer &buf) noexcept;

} // namespace udp

#endif
