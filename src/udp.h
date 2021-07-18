#ifndef SP_MAINLINE_DHT_UDP_H
#define SP_MAINLINE_DHT_UDP_H

#include "util.h"

namespace udp {
//=====================================
enum class Mode { BLOCKING, NONBLOCKING };

fd bind(Ipv4, Port, Mode) noexcept;

fd bind_v4(Port, Mode) noexcept;

fd bind_v4(Mode) noexcept;

fd
bind_unix(const char *file, Mode) noexcept;

fd
bind_unix_seq(const char *file, Mode) noexcept;

fd
connect_unix_seq(const char *file, Mode) noexcept;

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

namespace net {
//=====================================
bool
local(fd &, Contact &) noexcept;

bool
remote(fd &, Contact &) noexcept;

//=====================================
int
sock_read(fd &, sp::Buffer &) noexcept;

//=====================================
bool
sock_write(fd &, sp::Buffer &) noexcept;

//=====================================
} // namespace net

#endif
