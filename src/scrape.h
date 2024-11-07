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
seed_insert(dht::DHT &self, const sp::byte version[DHT_VERSION_LEN],
            const Contact &contact, const dht::NodeId &id);

bool
get_peers_close_nodes(dht::DHT &self,
                      const sp::UinArray<dht::IdContact> &values);

bool
sample_infohashes(dht::DHT &self, const Contact &con, uint32_t hours,
                  const sp::UinStaticArray<dht::Infohash, 128> &samples);
} // namespace scrape

#endif
