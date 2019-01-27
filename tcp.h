#ifndef SP_MAINLINE_DHT_TCP_H
#define SP_MAINLINE_DHT_TCP_H

#include "util.h"

namespace tcp {
//=====================================
bool
local(fd &, Contact &) noexcept;

//=====================================
enum class Mode { BLOCKING, NONBLOCKING };

fd
connect(const Contact &, Mode) noexcept;

//=====================================
int
send(fd &, /*OUT*/ sp::Buffer &) noexcept;

//=====================================
int
read(fd &, /*OUT*/ sp::Buffer &) noexcept;

//=====================================
} // namespace tcp

#endif
