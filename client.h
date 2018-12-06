#ifndef SP_MAINLINE_DHT_CLIENT_H
#define SP_MAINLINE_DHT_CLIENT_H

#include "shared.h"

namespace client {

Res
ping(dht::DHT &, sp::Buffer &, const dht::Node &) noexcept;

Res
find_node(dht::DHT &, sp::Buffer &, const Contact &, const dht::NodeId &,
          void *) noexcept;

Res
get_peers(dht::DHT &, sp::Buffer &, const Contact &, const dht::Infohash &,
          void *) noexcept;

} // namespace client

#endif
