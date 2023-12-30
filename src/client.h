#ifndef SP_MAINLINE_DHT_CLIENT_H
#define SP_MAINLINE_DHT_CLIENT_H

#include <io/fd.h>

#include "shared.h"

namespace client {
//=====================================
Res
ping(dht::DHT &, sp::Buffer &, const dht::Node &) noexcept;

//=====================================
Res
find_node(dht::DHT &, sp::Buffer &, const Contact &, const dht::NodeId &,
          void *) noexcept;

//=====================================
Res
get_peers(dht::DHT &, sp::Buffer &, const Contact &, const dht::Infohash &,
          void *) noexcept;

//=====================================
Res
sample_infohashes(dht::DHT &, sp::Buffer &, const Contact &,
                  const dht::Key &, void *) noexcept;

//=====================================
namespace priv {
template <typename Contacts>
Res
found(dht::DHT &, sp::Buffer &, const dht::Infohash &,
      const Contacts &) noexcept;
} // namespace priv

//=====================================
} // namespace client

#endif
