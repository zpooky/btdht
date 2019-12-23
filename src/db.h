#ifndef SP_MAINLINE_DHT_DB_H
#define SP_MAINLINE_DHT_DB_H

#include "shared.h"

namespace db {
//=====================================
dht::KeyValue *
lookup(dht::DHT &, const dht::Infohash &key) noexcept;

//=====================================
bool
insert(dht::DHT &, const dht::Infohash &key, const Contact &value,
       bool seed) noexcept;

//=====================================
void
mint_token(dht::DHT &, const Contact &, dht::Token &) noexcept;

//=====================================
bool
valid(dht::DHT &, dht::Node &, const dht::Token &) noexcept;

//=====================================
Timeout
on_awake_peer_db(dht::DHT &, sp::Buffer &) noexcept;

} // namespace db

#endif
