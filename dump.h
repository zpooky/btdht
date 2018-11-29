#ifndef SP_MAINLINE_DHT_DUMP_H
#define SP_MAINLINE_DHT_DUMP_H

#include "shared.h"

namespace sp {
bool
dump(const dht::DHT &, const char *path) noexcept;

bool
restore(dht::DHT &, const char *path) noexcept;

} // namespace sp

#endif
