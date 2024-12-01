#ifndef SP_MAINLINE_DHT_SCRAPE_H
#define SP_MAINLINE_DHT_SCRAPE_H

#include "module.h"
#include "shared.h"

namespace interface_setup {
bool
setup(dht::Modules &, bool setup_cb) noexcept;
}

namespace scrape {

bool
seed_insert(dht::DHT &self, const dht::Node &node);

bool
on_get_peers_nodes(dht::DHT &self, const sp::UinArray<dht::IdContact> &values);

bool
on_get_peers_peer(dht::DHT &self, const dht::Infohash &ih,
                  const sp::UinArray<Contact> &contacts);

bool
on_sample_infohashes(dht::DHT &self, const Contact &con, uint32_t hours,
                     const sp::UinStaticArray<dht::Infohash, 128> &samples);

void
publish(dht::DHT &self, const dht::Infohash &ih);

void
main_on_retire_good(void *ctx, const dht::Node &)noexcept;
} // namespace scrape

#endif
