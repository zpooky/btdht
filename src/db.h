#ifndef SP_MAINLINE_DHT_DB_H
#define SP_MAINLINE_DHT_DB_H

#include "shared.h"

namespace db {
//=====================================
dht::KeyValue *
lookup(dht::DHT &, const dht::Infohash &key) noexcept;

//=====================================
bool
insert(dht::DHT &, const dht::Infohash &key, const Contact &value, bool seed,
       const char *name) noexcept;

//=====================================
void
mint_token(dht::DHT &, const Contact &, dht::Token &) noexcept;

//=====================================
bool
is_valid_token(dht::DHT &, dht::Node &, const dht::Token &) noexcept;

//=====================================
Timestamp
on_awake_peer_db(dht::DHT &, sp::Buffer &) noexcept;

//=====================================
sp::UinStaticArray<dht::Infohash, 20> &
randomize_samples(dht::DHT &) noexcept;

//=====================================
std::uint32_t next_randomize_samples(const dht::DHT &self) noexcept;

//=====================================

} // namespace db

#endif
