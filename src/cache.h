#ifndef SP_MAINLINE_DHT_CACHE_H
#define SP_MAINLINE_DHT_CACHE_H

#include <shared.h>

namespace sp {
//========================
bool
init_cache(dht::DHT &) noexcept;

//========================
void
deinit_cache(dht::DHT &) noexcept;

//========================
} // namespace sp

#endif
