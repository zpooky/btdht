#ifndef SP_MAINLINE_DHT_CLIENT_H
#define SP_MAINLINE_DHT_CLIENT_H

#include "transaction.h"

namespace client { //
bool
ping(dht::DHT &, sp::Buffer &, const dht::Node &) noexcept;
}

#endif
