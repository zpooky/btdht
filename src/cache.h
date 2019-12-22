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
size_t
cache_read_min_idx(const dht::DHT &) noexcept;
size_t
cache_read_max_idx(const dht::DHT &)noexcept;
size_t
cache_contacts(const dht::DHT &)noexcept;
size_t
cache_write_idx(const dht::DHT &)noexcept;

//========================
} // namespace sp

#endif
