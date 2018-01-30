#ifndef SP_MAINLINE_DHT_DB_H
#define SP_MAINLINE_DHT_DB_H

#include "shared.h"

namespace db {

dht::KeyValue *
lookup(dht::DHT &, const dht::Infohash &) noexcept;

bool
insert(dht::DHT &, const dht::Infohash &, const Contact &) noexcept;

bool
valid(dht::DHT &, const dht::Token &) noexcept;

void
mint_token(dht::DHT &,dht::Node&, Contact&, dht::Token &) noexcept;

} // namespace lookup

#endif