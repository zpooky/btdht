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
//
static bool
has_sent_last_24h(DHT &self, const Ip &ip) {
  for (auto &bf : self.scrape_hour) {
    if (test(bf, ip)) {
      return true;
    }
  }
  return false;
}

static Timestamp
on_awake_scrape(DHT &self, sp::Buffer &buf) noexcept {
  (void)buf;
  for (size_t i = length(self.scrapes); i < capacity(self.scrapes) - 1; ++i) {
    dht::NodeId ih{};
    rand_key(self.random, ih);
    auto *rt = emplace(self.scrapes, self, ih);
    // TODO seed scrape with main routing table
    assertx(rt);
  }

  // if (length(self.scrape_hour) < 1) {
  while (!is_full(self.scrape_hour)) {
    auto *bf = emplace(self.scrape_hour, self.ip_hashers);
    assertx(bf);
  }

  while ((self.scrape_hour_time + sp::Hours(1)) <= self.now) {
    self.scrape_hour_i = self.scrape_hour_i + 1 % capacity(self.scrape_hour);
    clear(self.scrape_hour[self.scrape_hour_i]);
    self.scrape_hour_time = self.scrape_hour_time + sp::Hours(1);
  }

  size_t search_index = random(self.random) % length(self.scrapes);
  // TODO timeout is needed for this one as well (separate/shared)

  // if the best bucket share prefix with its nodeid to 16bits points
  //    and there are X amount of nodes in that bucket
  //        which have gt 24h sample_infohashes
  //        and support sample_infohashes (based on version that we store)
  //    then we can:
  //    1. send sample_infohashes
  //    2. for each infohash that we have not seen before get_peers
  //
  // bloomfilter[24] circular where each hour the id is incremnted and the last
  // circular index is cleared

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
