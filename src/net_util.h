#ifndef SP_MAINLINE_DHT_NET_UTIL_H
#define SP_MAINLINE_DHT_NET_UTIL_H

#include <netinet/in.h>

namespace sp {
//=====================================
::in_addr
network_for(::in_addr address, ::in_addr mask) noexcept;

//=====================================
bool
is_private_address(::in_addr ip, ::in_addr mask) noexcept;

//=====================================
} // namespace sp

#endif
