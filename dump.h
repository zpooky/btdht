#ifndef SP_MAINLINE_DHT_DUMP_H
#define SP_MAINLINE_DHT_DUMP_H

#include "shared.h"
#include "file.h"

namespace sp {

bool
dump(sp::Buffer &, const dht::DHT &, const file::Path &) noexcept;

} // namespace sp

#endif
