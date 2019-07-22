#ifndef SP_MAINLINE_DHT_BOOTSTRAP_H
#define SP_MAINLINE_DHT_BOOTSTRAP_H

#include "shared.h"

namespace dht {
//==========================================
void
bootstrap_insert(DHT &, const KContact &) noexcept;

void
bootstrap_insert(DHT &, const IdContact &) noexcept;

//==========================================
void
bootstrap_insert_force(DHT &, KContact &) noexcept;

//==========================================
void
bootstrap_reset(DHT &) noexcept;

//==========================================
void
bootstrap_reclaim(DHT &, dht::KContact *) noexcept;

//==========================================
dht::KContact *
bootstrap_alloc(DHT &, const dht::KContact &cur) noexcept;

//==========================================
} // namespace dht

#endif
