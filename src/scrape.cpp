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
on_awake_scrape(DHT &self, sp::Buffer &buf) noexcept {
  (void)buf;
  while (!is_full(self.scrapes)) {
    dht::NodeId ih{};
    rand_key(self.random, ih);
    auto *rt = emplace(self.scrapes, self, ih);
    // TODO seed scrape with main routing table
    assertx(rt);
  }

  // if the best bucket share prefix with its nodeid to 16bits points
  //    and there are X amount of nodes in that bucket
  //        which have gt 24h sample_infohashes
  //        and support sample_infohashes (based on version that we store)
  //    then we can:
  //    1. send sample_infohashes
  //    2. for each infohash that we have not seen before get_peers

  return self.now + sp::Seconds(60);
}
} // namespace dht

bool
interface_setup::setup(dht::Modules &modules, bool setup_cb) noexcept {
  if (setup_cb) {
    insert(modules.awake.on_awake, &dht::on_awake_scrape);
  }
  return true;
}
