#ifndef SP_MAINLINE_DHT_BOOTSTRAP_H
#define SP_MAINLINE_DHT_BOOTSTRAP_H

#include "shared.h"
#include <heap/binary.h>

namespace dht {
//==========================================
KContact *
bootstrap_insert(DHT &, const IdContact &) noexcept;

KContact *
bootstrap_insert(DHT &self, const Contact &contact) noexcept;

KContact *
bootstrap_insert(DHTMetaScrape &self, const IdContact &) noexcept;

KContact *
bootstrap_insert(DHTMetaScrape &self, const Contact &) noexcept;

KContact *
bootstrap_insert(DHTMetaScrape &self, const KContact &contact) noexcept;

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
