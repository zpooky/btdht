#include "scrape.h"
#include "shared.h"

namespace dht {
static void
rand_key(prng::xorshift32 &r, dht::NodeId &id) {
  for (std::size_t i = 0; i < sizeof(id.id); ++i) {
    id.id[i] = (sp::byte)random(r);
  }
}

// static void
// on_retire_good(void *tmp, const Contact &in) noexcept {
//   auto rt = (dht::DHTMetaRoutingTable *)tmp;
// }

static Timestamp
on_awake_scrape(DHT &dht, sp::Buffer &buf) noexcept {
  dht::DHTMetaScrape &self = dht.scrape;
  (void)buf;
  while (!is_full(self.routing_tables)) {
    dht::NodeId ih{};
    const bool timeout = false;
    rand_key(dht.random, ih);
    auto *rt = emplace(self.routing_tables, dht.random, dht.now, ih, dht.config,
                       timeout);
    assertx(rt);
    // auto *res = emplace(dht.routing_table.retire_good, on_retire_good, rt);
    // assertx(res);
  }

  return dht.now + sp::Seconds(60);
}
} // namespace dht

bool
interface_setup::setup(dht::Modules &modules, bool setup_cb) noexcept {
  if (setup_cb) {
    insert(modules.awake.on_awake, &dht::on_awake_scrape);
  }
  return true;
}
