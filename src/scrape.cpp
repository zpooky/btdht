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
static uint32_t
last_10m_average(const sp::UinArray<DHTMetaScrape *> &scrapes) {
  uint32_t result = 0;
  for (const auto scrape : scrapes) {
    assertx(scrape);
    result += scrape->box.diff;
  }
  result /= std::min(length(scrapes), std::size_t(1));
  return result;
}

static dht::DHTMetaScrape *
best_scrape_match(dht::DHT &self, const dht::Key &id) {
  dht::DHTMetaScrape *best_match = nullptr;
  std::size_t max_rank = 0;

  for (auto scrape : self.active_scrapes) {
    std::size_t r = rank(scrape->id, id);
    if (r >= max_rank) {
      max_rank = r;
      best_match = scrape;
    }
  }

  return best_match;
}

static void
scrape_retire_good(void *ctx, const Node &n) noexcept {
  assertx(ctx);
  auto scrape = (DHTMetaScrape *)ctx;
  auto &dht = scrape->dht;
  if (n.properties.support_sample_infohashes) {
    if (!has_sent_sample_infohash_last_24h(dht, n.contact.ip)) {
      if (scrape->upcoming_sample_infohashes > 0) {
        scrape->upcoming_sample_infohashes--;
      }
      assertx(!n.timeout_next);
      assertx(!n.timeout_priv);
      insert(dht.scrape_retire_good, n);
    }
  }
}

static void
swap_in_new(DHT &self, std::size_t idx) {
  auto old = self.active_scrapes[idx];
  assertx(old);

  dht::NodeId ih{};
  rand_key(self.random, ih);
  auto inx = new DHTMetaScrape(self, ih);
  emplace(inx->routing_table.retire_good, scrape_retire_good, inx);
  self.active_scrapes[idx] = inx;

  if (old) {
    dht::DHTMetaScrape *best_match = best_scrape_match(self, old->id.id);
    if (best_match) {
      std::size_t r = rank(best_match->id, best_match->id);
      for (const auto &value : old->bootstrap) {
        auto tmp = bootstrap_insert(
            *best_match, KContact(std::min(r, value.common), value.contact));
        if (!tmp) {
          bootstrap_insert(*inx, value.contact);
        }
      }
    }

    for_all_node(old->routing_table.root, [&](const dht::Node &n) {
      if (n.properties.support_sample_infohashes) {
        if (!has_sent_sample_infohash_last_24h(self, n.contact.ip)) {
          dht::DHTMetaScrape *best_match = best_scrape_match(self, old->id.id);
          if (best_match) {
            Node node(n.id, n.contact);
            node.properties.support_sample_infohashes =
                n.properties.support_sample_infohashes;
            dht::insert(best_match->routing_table, node);
          }
        }
      }
      return true;
    });
    self.statistics.scrape_swapped_ih++;
  }

  delete old;
} // namespace dht

static Timestamp
on_awake_scrape(DHT &self, sp::Buffer &buf) noexcept {
  // TODO is it some way of checking this, and atomic unlink
  if (!spbt_scrape_client_is_started(self.db.scrape_client)) {
    return self.now + sp::Seconds(60); // XXX
  }

  if (!is_empty(self.scrape_retire_good)) {
    for (auto &node : self.scrape_retire_good) {
      dht::DHTMetaScrape *best_match = best_scrape_match(self, node.id.id);
      if (best_match) {
        dht::insert(best_match->routing_table, node);
      }
    }
    clear(self.scrape_retire_good);
  }

  if (!is_full(self.active_scrapes)) {
    // XXX random take from self.active_scrapes
    for (size_t i = length(self.active_scrapes);
         i < capacity(self.active_scrapes); ++i) {
      dht::NodeId ih{};
      rand_key(self.random, ih);
      auto inx = new DHTMetaScrape(self, ih);
      emplace(inx->routing_table.retire_good, scrape_retire_good, inx);
      auto *rt = emplace(self.active_scrapes, inx);
      assertx(rt);
    }
  }

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
      ScrapeContext *closure = new ScrapeContext(needle);
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

  // if (self.scrape_active_sample_infhohash == 0)
  // I think this is difficult to reach
  {
    size_t scrape_idx = random(self.random) % length(self.active_scrapes);
    DHTMetaScrape *scrape = self.active_scrapes[scrape_idx];
    uint32_t published_since_last_check = 0;
    bool checked_ten_mins_ago = false;
    assertx(scrape);
    if (scrape) {
      if ((scrape->box.last_checked + sp::Minutes(10)) < self.now) {
        scrape->box.diff = published_since_last_check =
            scrape->stat.publish - scrape->box.publish;
        scrape->box.publish = scrape->stat.publish;
        scrape->box.last_checked = self.now;
        checked_ten_mins_ago = true;
      }
      if ((scrape->started + sp::Minutes(60)) < self.now) {
        if (checked_ten_mins_ago) {
          // if (1) { // TODO upcoming sample_infohashes < something (maybe some
          // kind of average)
          // TODO published < threshold (not just 10) (maybe some kind of
          // average)
          if (published_since_last_check < 10 &&
              published_since_last_check <
                  last_10m_average(self.active_scrapes)) {
            swap_in_new(self, scrape_idx);
          }
        }
        // }
      }
    }
  }

  if (!is_full(self.scrape_get_peers_ih) && tx::has_free_transaction(self)) {
    Config &cfg = self.config;
    size_t scrape_idx = random(self.random) % length(self.active_scrapes);
    DHTMetaScrape *scrape = self.active_scrapes[scrape_idx];
    assertx(scrape);
    if (scrape) {
      const dht::NodeId &needle = scrape->id;

      auto f = [&](auto &, Node &remote) {
        auto result = client::Res::OK;
        const Contact &c = remote.contact;

        if (remote.properties.support_sample_infohashes &&
            !has_sent_sample_infohash_last_24h(self, c.ip)
            // && shared_prefix(needle, remote.id) >= 10
        ) {
          if (is_full(self.scrape_get_peers_ih)) {
            return false;
          }
          result = client::sample_infohashes(self, buf, c, needle.id, nullptr);
          if (result == client::Res::OK) {
            scrape->stat.sent_sample_infohash++;
            self.scrape_active_sample_infhohash++;
            if (scrape->upcoming_sample_infohashes > 0) {
              --scrape->upcoming_sample_infohashes;
            }
          }
        } else {
          if (length(scrape->bootstrap) + 20 < capacity(scrape->bootstrap)) {
            result = client::find_node(self, buf, c, needle, nullptr);
          }
        }

        if (result == client::Res::OK) {
          remote.req_sent = self.now;
        }

        return result == client::Res::OK;
      };
      timeout::for_all_node(scrape->routing_table, cfg.refresh_interval, f);
    }
  }

  if (tx::has_free_transaction(self)) {
    dht::KContact cur;
    size_t scrape_idx = random(self.random) % length(self.active_scrapes);
    DHTMetaScrape *scrape = self.active_scrapes[scrape_idx];
    assertx(scrape);
    if (scrape) {
      while (tx::has_free_transaction(self) &&
             take_head(scrape->bootstrap, cur)) {
        auto result =
            client::find_node(self, buf, cur.contact, scrape->id, nullptr);
        if (result != client::Res::OK) {
          // inc_active_searches();
          break;
        }
      } // while
    }
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
scrape::seed_insert(dht::DHT &self, const dht::Node &in_node) {
#if 1
  dht::DHTMetaScrape *best_match = best_scrape_match(self, in_node.id.id);
  if (best_match) {
    dht::Node node(in_node.id, in_node.contact);
    node.properties.support_sample_infohashes =
        in_node.properties.support_sample_infohashes;
    // Note: node is prepended in the timeout list
    dht::insert(best_match->routing_table, node);

    if (node.properties.support_sample_infohashes) {
      ++best_match->upcoming_sample_infohashes;
    }
  }
#else
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
#endif

  return true;
}

bool
scrape::on_get_peers_nodes(dht::DHT &self,
                           const sp::UinArray<dht::IdContact> &values) {
  if (!is_empty(values) && !is_empty(self.active_scrapes)) {
#if 0
    // add all  based on first nodeid to the best scrape
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
#else
    for (const auto &value : values) {
      dht::DHTMetaScrape *best_match = best_scrape_match(self, value.id.id);
      if (best_match) {
        bootstrap_insert(*best_match, value);
      }
    }
  }
#endif
  return true;
}

bool
scrape::on_get_peers_peer(dht::DHT &self, const dht::Infohash &ih,
                          const sp::UinArray<Contact> &contacts) {
  dht::DHTMetaScrape *best_match = best_scrape_match(self, ih.id);
  if (best_match) {
    ++best_match->stat.get_peer_responses;
  }
  if (!spbt_has_infohash(self.db.scrape_client, ih)) {
    if (best_match) {
      ++best_match->stat.new_get_peer;
    }
    for (const auto &contact : contacts) {
      spbt_scrape_client_send(self.db.scrape_client, ih.id, contact);
    }
  }
  return true;
}

bool
scrape::on_sample_infohashes(
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
  dht::DHTMetaScrape *best_match = best_scrape_match(self, ih.id);

  auto f = stderr;
  // fprintf(f, "%s: max_rank:%zu ", __func__, max_rank);
  dht::print_hex(f, ih.id, sizeof(ih.id));
  fprintf(stderr, "\n");

  if (best_match) {
    ++best_match->stat.publish;
  }
}

void
scrape::main_on_retire_good(void *ctx, const dht::Node &n) noexcept {
  auto dht = (dht::DHT *)ctx;
  assertx(ctx);
  if (n.properties.support_sample_infohashes) {
    if (!has_sent_sample_infohash_last_24h(*dht, n.contact.ip)) {
      assertx(!n.timeout_next);
      assertx(!n.timeout_priv);
      insert(dht->scrape_retire_good, n);
    }
  }
}
