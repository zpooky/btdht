#ifndef SP_MAINLINE_DHT_DB_H
#define SP_MAINLINE_DHT_DB_H

#include "shared.h"

namespace db {

dht::KeyValue *
lookup(dht::DHT &, const dht::Infohash &key) noexcept;

bool
insert(dht::DHT &, const dht::Infohash &key, const Contact &value) noexcept;

// TODO document
void
mint_token(dht::DHT &, dht::Node &, Contact &, dht::Token &) noexcept;

bool
valid(dht::DHT &, dht::Node &, const dht::Token &) noexcept;

} // namespace db

#endif
