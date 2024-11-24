#include "scrape.h"
#include "bootstrap.h"
#include "client.h"
#include "dht.h"
#include "shared.h"
#include "timeout_impl.h"
#include "util.h"

#include <algorithm>

/*
 * TODO maintain how many:
 * - new infohash we get
 * - new nodes we get
 *
 * TODO
 * - rate of new infohash
 * - rate of new nodes
 */

namespace dht {
static void
rand_key(prng::xorshift32 &r, dht::NodeId &id) {
  for (std::size_t i = 0; i < sizeof(id.id); ++i) {
    id.id[i] = (sp::byte)random(r);
  }
}

static bool
has_sent_sample_infohash_last_24h(DHT &self, const Ip &ip) {
  for (auto &bf : self.scrape_hour) {
    if (test(bf, ip)) {
      return true;
    }
  }
  return false;
}

static bool
support_sample_infohashes(const sp::byte version[DHT_VERSION_LEN]) noexcept {
  if (!version) {
    return true;
  }
  if (std::memcmp(version, (const sp::byte *)"LT", 2) == 0) {
    /* # yes
     * 0x0207
     * 0x0206
     * 0x012F
     * 0x012E
     * 0x012B
     * 0x0102
     * # not
     * 0x0102
     * 0x0101
     * 0x0100
     * 0x0010
     * 0x000f
     */
    return true;
  }
  if (std::memcmp(version, (const sp::byte *)"\0\0", 2) == 0) {
    return false;
  }
  if (std::memcmp(version, (const sp::byte *)"lt", 2) == 0) {
    return false;
  }
  if (std::memcmp(version, (const sp::byte *)"sp", 2) == 0) {
    return true;
  }
  if (std::memcmp(version, (const sp::byte *)"UT", 2) == 0) {
    return false;
  }
  if (std::memcmp(version, (const sp::byte *)"ml", 2) == 0) {
    // 4:hex[6D6C0109]: 4(ml__)
    // 4:hex[6D6C010B]: 4(ml__)
    return true;
  }

  return false;
}

static Timestamp
on_awake_scrape(DHT &self, sp::Buffer &buf) noexcept {

  if (!spbt_scrape_client_is_started(self.db.scrape_client)) {
    return self.now + sp::Seconds(60); // XXX
  }

  if (!is_full(self.active_scrapes)) {
    for (size_t i = length(self.active_scrapes);
         i < capacity(self.active_scrapes) - 1; ++i) {
      dht::NodeId ih{};
      rand_key(self.random, ih);
      auto *rt = emplace(self.active_scrapes, self, ih);
      assertx(rt);
    }
  }

#if 0
  // if (length(self.scrape_hour) < 1) {
  while (!is_full(self.scrape_hour)) {
    auto *bf = emplace(self.scrape_hour, self.ip_hashers);
    assertx(bf);
  }
#endif

  if ((self.scrape_hour_time + sp::Hours(1 * capacity(self.scrape_hour))) <=
      self.now) {
    for (auto &f : self.scrape_hour) {
      clear(f);
    }
    self.scrape_hour_time = self.now;
  } else {
    while ((self.scrape_hour_time + sp::Hours(1)) <= self.now) {
      self.scrape_hour_idx =
          (self.scrape_hour_idx + 1) % capacity(self.scrape_hour);
      clear(self.scrape_hour[self.scrape_hour_idx]);
      self.scrape_hour_time = self.scrape_hour_time + sp::Hours(1);
    }
  }

  {
    bool result = true;
    while (!is_empty(self.scrape_get_peers_ih) &&
           tx::has_free_transaction(self) && result) {
      size_t last_idx = self.scrape_get_peers_ih.length - 1;
      auto &last = self.scrape_get_peers_ih[last_idx];
      auto needle = std::get<0>(last);
      if (spbt_has_infohash(self.db.scrape_client, needle)) {
        remove(self.scrape_get_peers_ih, last_idx);
        continue;
      }
      auto dest = std::get<1>(last);
      ScrapeContext *closure = new ScrapeContext(std::get<0>(last));
      result = client::get_peers(self, buf, dest, needle, closure) ==
               client::Res::OK;
      if (result) {
        remove(self.scrape_get_peers_ih, last_idx);
      } else {
        delete closure;
      }
      // node.req_sent = dht.now;
    }
  }

  if (!is_full(self.scrape_get_peers_ih) && tx::has_free_transaction(self)) {
    Config &cfg = self.config;
    size_t scrape_idx = random(self.random) % length(self.active_scrapes);
    DHTMetaScrape &scrape = self.active_scrapes[scrape_idx];
    const dht::NodeId &needle = scrape.id;

    auto f = [&](auto &, Node &remote) {
      auto result = client::Res::OK;
      const Contact &c = remote.contact;

      if (support_sample_infohashes(remote.version) &&
          !has_sent_sample_infohash_last_24h(self, c.ip)
          // && shared_prefix(needle, remote.id) >= 10
      ) {
        if (is_full(self.scrape_get_peers_ih)) {
          return false;
        }
        result = client::sample_infohashes(self, buf, c, needle.id, nullptr);
        if (result == client::Res::OK) {
          scrape.stat.sent_sample_infohash++;
        }
      } else {
        if (!is_full(scrape.bootstrap)) {
          result = client::find_node(self, buf, c, needle, nullptr);
        }
      }

      if (result == client::Res::OK) {
        remote.req_sent = self.now;
      }

      return result == client::Res::OK;
    };
    timeout::for_all_node(scrape.routing_table, cfg.refresh_interval, f);
  }

  if (tx::has_free_transaction(self)) {
    dht::KContact cur;
    size_t scrape_idx = random(self.random) % length(self.active_scrapes);
    DHTMetaScrape &scrape = self.active_scrapes[scrape_idx];
    while (tx::has_free_transaction(self) && take_head(scrape.bootstrap, cur)) {
      auto result =
          client::find_node(self, buf, cur.contact, scrape.id, nullptr);
      if (result != client::Res::OK) {
        // inc_active_searches();
        break;
      }
    } // while
  }

  // # v2
  // TODO DHTMetaScrape when to try a new infohash
  // - circular queu with the first of the queue is the highest shared id
  // - random select scrape:
  //   - run sample_infohashes if has_sent_last_24h(): otherwise if
  //   Node::req_sent < XX send find_node?

  // TODO timeout is needed for scrape as well
  // - use the same system as for original routing_table
  // - use reference counted Node which are shared between all routing tables
  // 1. foreach.timeout find_node(nodeId)
  //    TODO which nodeId
  // 2. if there are X amount of nodes in that routing table
  //    which have gt 24h sample_infohashes
  //    and support sample_infohashes (based on version that we store)
  // then we can:
  // 1. send sample_infohashes
  // 2. for each infohash that we have not seen before get_peers

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

bool
scrape::seed_insert(dht::DHT &self, const sp::byte version[DHT_VERSION_LEN],
                    const Contact &contact, const dht::NodeId &id) {
  for_each(self.active_scrapes, [&](auto &scrape) {
    // TODO 4 is arbitrary
    if (dht::shared_prefix(id, scrape.id) >=
        std::max(size_t(4), self.active_scrapes.capacity)) {

      dht::Node node(id, contact);
      if (version) {
        memcpy(node.version, version, sizeof(node.version));
      }
      // XXX add with a timeout so that we don't spam the same node
      dht::insert(scrape.routing_table, node);
    }
  });

  return true;
}

bool
scrape::get_peers_nodes(dht::DHT &self,
                        const sp::UinArray<dht::IdContact> &values) {
  if (!is_empty(values) && !is_empty(self.active_scrapes)) {
    const dht::IdContact *first = values.begin();
    dht::DHTMetaScrape *best_match = nullptr;
    std::size_t max_rank = 0;

    for (auto &scrape : self.active_scrapes) {
      std::size_t r = rank(first->id, scrape.id);
      if (r > max_rank) {
        max_rank = r;
        best_match = &scrape;
      }
    }

    if (best_match) {
      for (const auto &value : values) {
        bootstrap_insert(*best_match, value);
      }
    }
  }

  return true;
}

bool
scrape::get_peers_peer(dht::DHT &self, const dht::Infohash &ih,
                       const Contact &contact) {
  return spbt_scrape_client_send(self.db.scrape_client, ih.id, contact);
}

bool
scrape::sample_infohashes(
    dht::DHT &self, const Contact &con, uint32_t hours,
    const sp::UinStaticArray<dht::Infohash, 128> &samples) {
#if 0
#else
  size_t h = (self.scrape_hour_idx + capacity(self.scrape_hour) + hours + 1) %
             capacity(self.scrape_hour);
#endif
  insert(self.scrape_hour[h], con.ip);

  for (const dht::Infohash &ih : samples) {
    if (!spbt_has_infohash(self.db.scrape_client, ih)) {
      insert(self.scrape_get_peers_ih,
             std::tuple<dht::Infohash, Contact>(ih, con));
    }
  }
  return true;
}

void
scrape::publish(dht::DHT &self, const dht::Infohash &ih) {
  dht::DHTMetaScrape *best_match = nullptr;
  std::size_t max_rank = 0;
  for (auto &scrape : self.active_scrapes) {
    std::size_t r = rank(scrape.id, ih.id);
    if (r > max_rank) {
      max_rank = r;
      best_match = &scrape;
    }
  }

  if (best_match) {
    ++best_match->stat.publish;
  }
}
