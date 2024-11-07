#ifndef SP_MAINLINE_DHT_BOOTSTRAP_H
#define SP_MAINLINE_DHT_BOOTSTRAP_H

#include "shared.h"
#include <heap/binary.h>

namespace dht {
//==========================================
void
bootstrap_insert(DHTMetaBootstrap &, heap::MaxBinary<KContact> &contacts,
                 const KContact &) noexcept;

void
bootstrap_insert(DHT &, const IdContact &) noexcept;

//==========================================
void
bootstrap_insert_force(DHT &, KContact &) noexcept;

//==========================================
void
bootstrap_reclaim(DHT &, dht::KContact *) noexcept;

//==========================================
dht::KContact *
bootstrap_alloc(DHT &, const dht::KContact &cur) noexcept;

//==========================================
bool
bootstrap_take_head(DHT &, dht::KContact &out) noexcept;

//==========================================
} // namespace dht

#endif
