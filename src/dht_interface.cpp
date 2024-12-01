#include "dht_interface.h"
#include <cmath>
#include <inttypes.h>

#include "Log.h"
#include "bencode.h"
#include "bootstrap.h"
#include "client.h"
#include "db.h"
#include "decode_bencode.h"
#include "dht.h"
#include "krpc.h"
#include "krpc_parse.h"
#include "scrape.h"
#include "search.h"
#include "spbt_scrape_client.h"
#include "timeout.h"
#include "timeout_impl.h"
#include "transaction.h"

#include <sha1.h>

#include <algorithm>
#include <cstring>
#include <prng/util.h>
#include <string/ascii.h>
#include <util/assert.h>
#include <utility>

namespace dht {
static Timestamp
on_awake_find_nodes(DHT &dht, sp::Buffer &out) noexcept;

static Timestamp
on_awake_ping(DHT &, sp::Buffer &) noexcept;

static Timestamp
on_awake_eager_tx_timeout(DHT &, sp::Buffer &) noexcept;

static Timestamp
on_awake_peer_db_glue(DHT &self, sp::Buffer &buf) noexcept {
  assertxs(self.now == self.db.now, std::uint64_t(self.now),
           std::uint64_t(self.db.now));
  return db::on_awake_peer_db(self.db, buf);
}
} // namespace dht

//=====================================
namespace interface_dht {
bool
setup(dht::Modules &modules, bool setup_cb) noexcept {
  std::size_t &i = modules.length;
  ping::setup(modules.modules[i++]);
  find_node::setup(modules.modules[i++]);
  get_peers::setup(modules.modules[i++]);
  announce_peer::setup(modules.modules[i++]);
  sample_infohashes::setup(modules.modules[i++]);
  error::setup(modules.modules[i++]);

  if (setup_cb) {
#if 1
    insert(modules.awake.on_awake, &dht::on_awake_peer_db_glue);
#endif
    insert(modules.awake.on_awake, &dht::on_awake_find_nodes);
    insert(modules.awake.on_awake, &dht::on_awake_ping);
    insert(modules.awake.on_awake, &dht::on_awake_eager_tx_timeout);
  }

  return true;
}
} // namespace interface_dht

//=====================================

namespace dht {
template <typename F>
static void
for_each(Node *node, F f) noexcept {
  while (node) {
    f(node);
    node = node->timeout_next;
  }
}

static void
inc_outstanding(Node &node) noexcept {
  const std::uint8_t max = ~std::uint8_t(0);
  if (node.outstanding != max) {
    ++node.outstanding;
  }
}

/*pings*/
static Timestamp
on_awake_ping(DHT &self, sp::Buffer &out) noexcept {
  Config &cfg = self.config;

  /* Send ping to nodes */
  auto f = [&out, &self](auto &, auto &node) {
    bool result = client::ping(self, out, node) == client::Res::OK;
    if (result) {
      inc_outstanding(node);
      // timeout::update_send(dht, node);

      /* Fake update activity otherwise if all nodes have to
       * same timeout we will spam out pings, ex: 3 nodes timed
       * out, send ping, append, get the next timeout date,
       * since there is only 3 in the queue and we will
       * immediately awake and send ping  to the same 3 nodes
       */
      node.req_sent = self.now;
    }

    return result;
  };
  timeout::for_all_node(self.routing_table, cfg.refresh_interval, f, &self);

  /* Calculate next timeout based on the head if the timeout list which is in
   * sorted order where to oldest node is first in the list.
   */
  Node *const tHead = self.tb.timeout->timeout_node;
  if (tHead) {
    self.tb.timeout->timeout_next = tHead->req_sent + cfg.refresh_interval;

    if (self.now >= self.tb.timeout->timeout_next) {
      self.tb.timeout->timeout_next = self.now + cfg.min_timeout_interval;
    }

  } else {
    /* timeout queue is empty */
    self.tb.timeout->timeout_next = self.now + cfg.refresh_interval;
  }

  assertx(self.tb.timeout->timeout_next > self.now);
  return self.tb.timeout->timeout_next;
}

static Timestamp
on_awake_eager_tx_timeout(DHT &self, sp::Buffer &) noexcept {
  Config &cfg = self.config;

  tx::eager_tx_timeout(self);
  auto head = tx::next_timeout(self);
  if (!head) {
    return self.now + cfg.refresh_interval;
  }
  auto next = head->sent + cfg.transaction_timeout;
  assertxs(next > self.now, uint64_t(head->sent),
           uint64_t(head->sent + cfg.transaction_timeout), uint64_t(self.now));
  return next;
}

static Timestamp
awake_look_for_nodes(dht::DHT &self, DHTMetaRoutingTable &rt, sp::Buffer &out,
                     std::size_t missing_contacts, bool use_bootstrap) {
  Config &cfg = self.config;
  std::size_t now_sent = 0;
  auto now = self.now;

  auto inc_active_searches = [&missing_contacts, &now_sent]() {
    missing_contacts -= std::min(missing_contacts, dht::Bucket::K);
    now_sent++;
  };

  // XXX self should not be in bootstrap list
  // XXX if no good node is available try bad/questionable nodes

  auto f = [inc_active_searches, &out, &now, &self](auto &, Node &remote) {
    auto result = client::Res::OK;
    // if (dht::is_good(self, remote)) {
    const Contact &c = remote.contact;
    dht::NodeId &sid = self.id;

    result = client::find_node(self, out, c, /*search*/ sid, nullptr);
    if (result == client::Res::OK) {
      inc_outstanding(remote);
      inc_active_searches();
      remote.req_sent = now;
    }
    // }

    return result == client::Res::OK;
  };
  timeout::for_all_node(self.routing_table, cfg.refresh_interval, f, &self);

  if (use_bootstrap) {
    dht::KContact cur;
    while (tx::has_free_transaction(self) && bootstrap_take_head(self, cur)) {
      auto closure = bootstrap_alloc(self, cur);
      auto result = client::find_node(self, out, cur.contact, rt.id, closure);
      if (result == client::Res::OK) {
        inc_active_searches();
      } else {
        bootstrap_insert_force(self, cur);
        bootstrap_reclaim(self, closure);
        break;
      }
    } // while
  }

  if (missing_contacts > 0) {
    Timestamp next = tx::next_available(self);
    if (next > now) {
      return next;
    } else {
      return now + cfg.transaction_timeout;
    }
  }

  return now + cfg.refresh_interval;
}

static Timestamp
on_awake_find_nodes(DHT &self, sp::Buffer &out) noexcept {
  Timestamp result(self.now + self.config.refresh_interval);

  auto percentage = [](std::uint32_t t, std::uint32_t c) {
    return double(c) / (double(t) / 100.);
  };

  assertx(self.now == self.routing_table.now);
  assertx(self.now == self.db.now);

  auto good = nodes_good(self.routing_table);
  const uint32_t all = dht::max_routing_nodes(self.routing_table);
  uint32_t look_for = all - good;

  const auto cur = percentage(all, good);
#if 0
  printf("good[%u], total[%u], bad[%u], look_for[%u], "
         "config.seek[%zu%s], "
         "cur[%.2f%s], max[%u], self.root[%zd], bootstraps[%zu]\n",
         good, nodes_total(self), nodes_bad(self), look_for, //
         self.config.percentage_seek, "%",                  //
         cur, "%", all, self.root ? self.root->depth : 0, length(self.bootstrap));
#endif

  if (cur < (double)self.config.percentage_seek) {
    // TODO if we can't mint new tx then next should be calculated base on when
    // soonest next tx timesout is so we can directly reuse it. (it should not
    // be the config.refresh_interval of 15min if we are under conf.p_seek)
    auto awake_next =
        awake_look_for_nodes(self, self.routing_table, out, look_for, true);
    result = std::min(result, awake_next);
    logger::awake::contact_scan(self);
  }

  assertxs(result > self.now, uint64_t(result), uint64_t(self.now));
  return result;
}

static dht::Node *
dht_insert(DHT &self, const Node &contact) noexcept {
  dht::Node *result = dht::insert(self.routing_table, contact);
  scrape::seed_insert(self, contact);

  return result;
}

static Node *
dht_activity(dht::MessageContext &ctx, const dht::NodeId &senderId) noexcept {
  krpc::ParseContext &pctx = ctx.pctx;
  DHT &self = ctx.dht;

  Node *result = find_node(self.routing_table, senderId);
  if (result) {
    if (pctx.remote_version[0] != '\0') {
      result->properties.support_sample_infohashes =
          dht::support_sample_infohashes(pctx.remote_version);
    }
    result->properties.is_readonly = ctx.read_only;
    if (!result->properties.is_good) {
      result->properties.is_good = true;
      result->outstanding = 0;
      assertx(self.routing_table.bad_nodes > 0);
      self.routing_table.bad_nodes--;
    }
  } else {
    if (!ctx.read_only) {
      Node contact(senderId, ctx.remote, self.now);
      contact.properties.support_sample_infohashes =
          dht::support_sample_infohashes(pctx.remote_version);
      result = dht_insert(self, contact);
    }
  }

  return result;
}

} // namespace dht

static void
handle_ip_election(dht::MessageContext &ctx,
                   const dht::NodeId &remote_id) noexcept {
  if (bool(ctx.pctx.ip_vote)) {
    if (is_valid_strict_id(ctx.remote.ip, remote_id)) {
      const Contact &v = ctx.pctx.ip_vote.get();
      auto &dht = ctx.dht;
      vote(dht.election, ctx.remote, v);
    }
  }
}

static void
__bootstrap_insert(dht::DHT &self, const dht::NodeId &id,
                   const Contact &remote) noexcept {
  std::size_t max_rank = 4;
  dht::DHTMetaScrape *max = nullptr;
  for (auto scrape : self.active_scrapes) {
    assertx(scrape);
    auto scrape_rank = rank(scrape->routing_table.id, id);
    if (scrape_rank >= max_rank) {
      // XXX check boostrap heap if full and if last element is less than tmp
      max_rank = scrape_rank;
      max = scrape;
    }
  }
  if (max) {
    bootstrap_insert(*max, dht::IdContact(id, remote));
  } else {
    bootstrap_insert(self, dht::IdContact(id, remote));
  }
}

template <typename F>
static bool
message(dht::MessageContext &ctx, const dht::NodeId &sender, F f) noexcept {
  dht::DHT &self = ctx.dht;

  // if (!dht::is_valid(sender)) {
  //   logger::receive::parse::invalid_node_id(
  //       ctx, ctx.query, ctx.pctx.remote_version,
  //       sizeof(ctx.pctx.remote_version), sender);
  //   return false;
  // }

  /*request from self*/
  if (self.id == sender.id) {
    logger::receive::parse::self_sender(ctx);
    return false;
  }

  if (!dht::is_valid(sender)) {
    dht::Node dummy{};
    f(dummy);
  } else if (ctx.domain == dht::Domain::Domain_private) {
    dht::Node dummy{};
    f(dummy);
  } else {
    assertx(ctx.remote.port != 0);
    if (dht::is_blacklisted(self, ctx.remote)) {
      return true;
    }

    dht::Node *contact = dht_activity(ctx, sender);

    handle_ip_election(ctx, sender);
    if (contact) {
      contact->remote_activity = self.now;
      f(*contact);
    } else {
      if (!ctx.read_only) {
        __bootstrap_insert(self, sender, ctx.remote);
      }
      dht::Node n{sender, ctx.remote};
      f(n);
    }
  }

  return true;
}

//===========================================================
// Ping
//===========================================================
namespace ping {
static bool
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  logger::receive::req::ping(ctx);

  message(ctx, sender, [&ctx](auto &) {
    dht::DHT &dht = ctx.dht;
    krpc::response::ping(ctx.out, ctx.transaction, dht.id);
  });
  // XXX response

  return true;
} // ping::handle_request()

static bool
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  logger::receive::res::ping(ctx);

  message(ctx, sender, [](auto &node) { //
    node.outstanding = 0;
  });

  return true;
} // ping::handle_response()

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  krpc::PingResponse res;
  if (krpc::parse_ping_response(ctx, res)) {
    return handle_response(ctx, res.sender);
  }
  return false;
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::PingRequest req;
  if (krpc::parse_ping_request(ctx, req)) {
    return handle_request(ctx, req.sender);
  }
  return false;
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, const Timestamp &sent,
           void *arg) noexcept {
  logger::transmit::error::ping_response_timeout(dht, tx, sent);
  assertx(arg == nullptr);
}

void
setup(dht::Module &module) noexcept {
  module.query = "ping";
  module.request = on_request;
  module.response = on_response;
  module.response_timeout = on_timeout;
}
} // namespace ping

//===========================================================
// find_node
//===========================================================
namespace find_node {
static bool
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender,
               const dht::NodeId &search, bool n4, bool n6) noexcept {
  logger::receive::req::find_node(ctx);

  message(ctx, sender, [&](auto &) {
    dht::DHT &dht = ctx.dht;
    constexpr std::size_t capacity = 8;
    std::size_t length4 = 0;

    const krpc::Transaction &t = ctx.transaction;
    dht::Node *result[capacity] = {nullptr};
    const dht::Node **nodes4 = nullptr;
    if (n4) {
      dht::multiple_closest(dht.routing_table, search, result);
      nodes4 = (const dht::Node **)&result;
      length4 = capacity;
    }

    krpc::response::find_node(ctx.out, t, dht.id, n4, nodes4, length4, n6);
  });

  return true;
} // find_node::handle_request()

template <typename ContactType>
static bool
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                /*const sp::list<dht::Node>*/ ContactType &contacts) noexcept {
  static_assert(
      std::is_same<dht::IdContact, typename ContactType::value_type>::value,
      "");
  logger::receive::res::find_node(ctx);

  message(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const dht::IdContact &contact) {
      dht::DHT &dht = ctx.dht;

      __bootstrap_insert(dht, contact.id, contact.contact);
    });
  });

  return true;
} // find_node::handle_response()

static void
handle_response_timeout(dht::DHT &dht, void *&closure) noexcept {
  if (closure) {
    bootstrap_reclaim(dht, (dht::KContact *)closure);
    closure = nullptr;
  }
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, const Timestamp &sent,
           void *closure) noexcept {
  logger::transmit::error::find_node_response_timeout(dht, tx, sent);
  if (closure) {
    auto *bs = (dht::KContact *)closure;
    bootstrap_insert_force(dht, *bs);
  }

  handle_response_timeout(dht, closure);
} // find_node::on_timeout

static bool
on_response(dht::MessageContext &ctx, void *closure) noexcept {
  krpc::FindNodeResponse res;
  dht::DHT &dht = ctx.dht;

  dht::KContact cap_copy;
  dht::KContact *cap_ptr = nullptr;
  if (closure) {
    auto *bs = (dht::KContact *)closure;

    // make a copy of the capture so we can delete it
    cap_copy = *bs;
    cap_ptr = &cap_copy;
  }
  handle_response_timeout(ctx.dht, closure);

  if (krpc::parse_find_node_response(ctx, res)) {

    if (is_empty(res.nodes)) {
      if (cap_ptr) {
        if (nodes_good(dht.routing_table) < 100) {
          /* Only remove bootstrap node if we have gotten some nodes from it */
          bootstrap_insert_force(dht, *cap_ptr);
        }
      }
    }

    return handle_response(ctx, res.id, res.nodes);
  }

  return false;
} // find_node::on_response

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::FindNodeRequest req;
  if (krpc::parse_find_node_request(ctx, req)) {
    return handle_request(ctx, req.sender, req.target, req.n4, req.n6);
  }
  return false;
} // find_node::on_request

void
setup(dht::Module &module) noexcept {
  module.query = "find_node";
  module.request = on_request;
  module.response = on_response;
  module.response_timeout = on_timeout;
}

} // namespace find_node

//===========================================================
// get_peers
//===========================================================
namespace get_peers {
template <int SIZE>
static void
bloomfilter_insert(uint8_t (&bloom)[SIZE], const Ip &ip) noexcept {
  const int m = SIZE * 8;
  uint8_t hash[20]{0};
  SHA1_CTX ctx{};

  SHA1Init(&ctx);

  if (ip.type == IpType::IPV4) {
    // both implementations work
    // the standard want for example the address 192.168.0.1 to be proccesed in
    // the order 192,168,0,1
#if 0
    Ipv4 ipv4 = ip.ipv4;
    unsigned char c0 = (ipv4 & (uint32_t(0xff) << 24)) >> 24;
    unsigned char c1 = (ipv4 & (uint32_t(0xff) << 16)) >> 16;
    unsigned char c2 = (ipv4 & (uint32_t(0xff) << 8)) >> 8;
    unsigned char c3 = (ipv4 & (0xff));
    // printf("%d.%d.%d.%d\n", c0, c1, c2, c3);
    SHA1Update(&ctx, &c0, 1);
    SHA1Update(&ctx, &c1, 1);
    SHA1Update(&ctx, &c2, 1);
    SHA1Update(&ctx, &c3, 1);
#else
    Ipv4 tmp = htonl(ip.ipv4);
    const uint8_t *raw = (const uint8_t *)&tmp;
    SHA1Update(&ctx, raw, (uint32_t)sizeof(ip.ipv4));
#endif
  } else {
    assertx(false);
    // const void *raw = (const void *)&ip.ipv6.raw;
    // SHA1Update(&ctx, (const uint8_t *)raw, (uint32_t)sizeof(ip.ipv6));
  }
  SHA1Final(hash, &ctx);

  int index1 = hash[0] | hash[1] << 8;
  int index2 = hash[2] | hash[3] << 8;

  // truncate index to m (11 bits required)
  index1 %= m;
  index2 %= m;

  bloom[index1 / 8] = uint8_t(bloom[index1 / 8] | (0x01 << (index1 % 8)));
  bloom[index2 / 8] = uint8_t(bloom[index2 / 8] | (0x01 << (index2 % 8)));
}

static bool
handle_get_peers_request(dht::MessageContext &ctx, const dht::NodeId &id,
                         const dht::Infohash &search, sp::maybe<bool> m_noseed,
                         bool scrape, bool n4, bool n6) noexcept {
  logger::receive::req::get_peers(ctx);

  message(ctx, id, [&](auto &) {
    dht::DHT &dht = ctx.dht;
    dht::Token token;
    db::mint_token(dht.db, ctx.remote, token);

    const krpc::Transaction &t = ctx.transaction;

    const dht::KeyValue *const result = db::lookup(dht.db, search);
    if (scrape) {
      // http://www.bittorrent.org/beps/bep_0033.html
      uint8_t seeds[256]{};
      uint8_t peers[256]{};

      if (result) {
        for_each(result->peers, [&](const dht::Peer &cur) {
          if (cur.seed) {
            bloomfilter_insert(seeds, cur.contact.ip);
          } else {
            bloomfilter_insert(peers, cur.contact.ip);
          }
        });

        krpc::response::get_peers_scrape(ctx.out, t, dht.id, token, seeds,
                                         peers);
        return;
      }
    } else {
      if (result) {
        clear(dht.recycle_contact_list);
        auto &filtered = dht.recycle_contact_list;
        for_each(result->peers, [&filtered, &m_noseed](const dht::Peer &cur) {
          if (m_noseed) {
            if ((cur.seed == false && m_noseed.get() == true) ||
                (cur.seed == true && m_noseed.get() == false)) {
              insert(filtered, cur.contact);
            }
          } else {
            insert(filtered, cur.contact);
          }
        });

        if (!is_empty(filtered)) {
          krpc::response::get_peers_peers(ctx.out, t, dht.id, token, filtered);
          return;
        }
      }
    }

    constexpr std::size_t capacity = 8;
    dht::Node *closest[capacity] = {nullptr};
    const dht::Node **nodes4 = nullptr;
    std::size_t length4 = 0;
    if (n4) {
      dht::multiple_closest(dht.routing_table, search, closest);
      nodes4 = (const dht::Node **)&closest;
      length4 = capacity;
    }

    krpc::response::get_peers(ctx.out, t, dht.id, token, n4, nodes4, length4,
                              n6);
  });

  return true;
} // namespace get_peers

template <typename ResultType, typename ContactType>
static bool
search_handle_response(
    dht::MessageContext &ctx, const dht::NodeId &sender,
    const dht::Token &, // XXX store token to used for announce
    const ResultType /*sp::list<Contact>*/ &values,
    const ContactType /*sp::list<dht::IdContact>*/ &nodes,
    dht::Search &search) noexcept {
  static_assert(std::is_same<Contact, typename ResultType::value_type>::value,
                "");
  static_assert(
      std::is_same<dht::IdContact, typename ContactType::value_type>::value,
      "");

  logger::receive::res::get_peers(ctx);
  dht::DHT &dht = ctx.dht;

  message(ctx, sender, [&](auto &) {
    /* Sender returns the its closest nodes for search */
    for_each(nodes, [&](const dht::IdContact &contact) {
      search_insert(search, contact);

      __bootstrap_insert(dht, contact.id, contact.contact);
    });

    /* Sender returns matching values for search query */
    for_each(values, [&](const Contact &c) {
      search_insert_result(search, c);
      //
    });
    scrape::on_get_peers_peer(dht, search.search, values);
  });

  return true;
} // namespace get_peers

template <typename ResultType, typename ContactType>
static bool
scrape_handle_response(
    dht::MessageContext &ctx, const dht::NodeId &sender,
    const dht::Token &, // XXX store token to used for announce
    const ResultType /*sp::list<Contact>*/ &values,
    const ContactType /*sp::list<dht::IdContact>*/ &nodes,
    const dht::Infohash &ih) noexcept {
  static_assert(std::is_same<Contact, typename ResultType::value_type>::value,
                "");
  static_assert(
      std::is_same<dht::IdContact, typename ContactType::value_type>::value,
      "");

  logger::receive::res::get_peers(ctx);
  dht::DHT &dht = ctx.dht;

  message(ctx, sender, [&](auto &) {
    /* Sender returns the its closest nodes for search */
    scrape::on_get_peers_nodes(dht, nodes);

    /* Sender returns matching values for search query */
    scrape::on_get_peers_peer(dht, ih, values);
  });

  return true;
} // namespace get_peers

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, const Timestamp &sent,
           void *ctx) noexcept {
  auto closure = (dht::get_peers_context *)ctx;
  logger::transmit::error::get_peers_response_timeout(dht, tx, sent);
  if (auto sctx = dynamic_cast<dht::SearchContext *>(closure)) {
    if (sctx) {
      search_decrement(sctx);
    }
  } else if (auto sctx = dynamic_cast<dht::ScrapeContext *>(closure)) {
    delete sctx;
  } else {
    assertx(false);
  }
} // get_peers::on_timeout

static bool
on_response(dht::MessageContext &ctx, void *tmp) noexcept {
  auto closure = (dht::get_peers_context *)tmp;
  krpc::GetPeersResponse res;
  // assertx(closure);
  if (!closure) {
    return true;
  }

  if (krpc::parse_get_peers_response(ctx, res)) {
    if (auto sctx = dynamic_cast<dht::SearchContext *>(closure)) {
      dht::Search *search = search_find(ctx.dht.searches, sctx);
      search_decrement(sctx);

      if (!search) {
        return true;
      }
      return search_handle_response(ctx, res.id, res.token, res.values,
                                    res.nodes, *search);
    } else if (auto sctx = dynamic_cast<dht::ScrapeContext *>(closure)) {
      if (!sctx) {
        return true;
      }
      dht::Infohash ih = sctx->infohash;
      delete sctx;
      return scrape_handle_response(ctx, res.id, res.token, res.values,
                                    res.nodes, ih);
    } else {
      assertx(false);
    }
  }

  return false;
} // get_peers::on_response

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::GetPeersRequest req;
  if (krpc::parse_get_peers_request(ctx, req)) {
    return handle_get_peers_request(ctx, req.sender, req.infohash, req.noseed,
                                    req.scrape, req.n4, req.n6);
  }
  return false;
} // get_peers::on_request

void
setup(dht::Module &module) noexcept {
  module.query = "get_peers";
  module.request = on_request;
  module.response = on_response;
  module.response_timeout = on_timeout;
}
} // namespace get_peers

//===========================================================
namespace announce_peer {
static bool
handle_announce_peer_request(dht::MessageContext &ctx,
                             const dht::NodeId &sender, bool implied_port,
                             const dht::Infohash &infohash, Port port,
                             const dht::Token &token, bool seed,
                             const char *name) noexcept {
  logger::receive::req::announce_peer(ctx);

  dht::DHT &dht = ctx.dht;
  message(ctx, sender, [&](auto &from) {
    if (db::is_valid_token(dht.db, from, token)) {
      Contact peer(ctx.remote);
      if (implied_port || port == 0) {
        peer.port = ctx.remote.port;
      } else {
        peer.port = port;
      }

      db::insert(dht.db, infohash, peer, seed, name);
      krpc::response::announce_peer(ctx.out, ctx.transaction, dht.id);
    } else {
      const char *msg = "Announce_peer with wrong token";
      krpc::response::error(ctx.out, ctx.transaction,
                            krpc::Error::protocol_error, msg);
    }
  });

  return true;
}

static bool
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  logger::receive::res::announce_peer(ctx);

  message(ctx, sender, [](auto &) { //

  });

  return true;
}

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  krpc::AnnouncePeerResponse res;
  if (krpc::parse_announce_peer_response(ctx, res)) {
    return handle_response(ctx, res.id);
  }
  return false;
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::AnnouncePeerRequest req;
  if (krpc::parse_announce_peer_request(ctx, req)) {
    char name_abr[128]{'\0'};
    if (req.name) {
      memcpy(name_abr, req.name, std::min(req.name_len, sizeof(name_abr)));
      name_abr[127] = '\0';
    }

    return handle_announce_peer_request(ctx, req.sender, req.implied_port,
                                        req.infohash, req.port, req.token,
                                        req.seed, name_abr);
  }
  return false;
}

void
setup(dht::Module &module) noexcept {
  module.query = "announce_peer";
  module.request = on_request;
  module.response = on_response;
}
} // namespace announce_peer

//===========================================================
namespace sample_infohashes {
static bool
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender,
               const dht::Key &target) noexcept {
  logger::receive::req::sample_infohashes(ctx);

  message(ctx, sender, [&ctx, &target](auto &) {
    dht::DHT &self = ctx.dht;
    constexpr std::size_t capacity = 8;

    dht::Node *nodes[capacity] = {nullptr};
    dht::multiple_closest(self.routing_table, target, nodes);

    std::uint32_t num = self.db.length_lookup_table;
    sp::UinStaticArray<dht::Infohash, 20> &samples =
        db::randomize_samples(self.db);
    std::uint32_t interval = db::next_randomize_samples(self.db, self.now);

    krpc::response::sample_infohashes(ctx.out, ctx.transaction, self.id,
                                      interval, (const dht::Node **)nodes,
                                      capacity, num, samples);
  });
  // XXX response

  return true;
}

static bool
handle_response(dht::MessageContext &ctx,
                const krpc::SampleInfohashesResponse &res) noexcept {
  dht::DHT &self = ctx.dht;
  // TODO logger::receive::res::sample_infohashes(ctx);

  for (const std::tuple<dht::NodeId, Contact> &e : res.nodes) {
    const Contact &c = std::get<1>(e);
    if (c.ip.type == IpType::IPV4 && c.ip.ipv4 && c.port) {
      const auto &nodeId = std::get<0>(e);
      __bootstrap_insert(self, nodeId, c);
    }
  }

  uint32_t hours = (uint32_t)std::ceil((double)res.interval / 60. / 60.);
  scrape::on_sample_infohashes(self, ctx.remote, hours, res.samples);

  return true;
}

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  dht::DHT &self = ctx.dht;
  krpc::SampleInfohashesResponse res;

  assertx(self.scrape_active_sample_infhohash > 0);
  self.scrape_active_sample_infhohash--;

  if (krpc::parse_sample_infohashes_response(ctx.in, res)) {
    return handle_response(ctx, res);
  }
  return false;
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::SampleInfohashesRequest req;
  if (krpc::parse_sample_infohashes_request(ctx.in, req)) {
    return handle_request(ctx, req.sender, req.target);
  }
  return false;
}

static void
on_timeout(dht::DHT &self, const krpc::Transaction &tx, const Timestamp &sent,
           void *arg) noexcept {
  logger::transmit::error::sample_infohashes_response_timeout(self, tx, sent);

  assertx(self.scrape_active_sample_infhohash > 0);
  self.scrape_active_sample_infhohash--;

  assertx(arg == nullptr);
}
} // namespace sample_infohashes

bool
sample_infohashes::setup(dht::Module &m) noexcept {
  m.query = "sample_infohashes";
  m.request = on_request;
  m.response = on_response;
  m.response_timeout = on_timeout;
  return true;
}

//===========================================================
// error
//===========================================================
namespace error {
static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  logger::receive::res::error(ctx);

  return true;
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  logger::receive::req::error(ctx);

  krpc::Error e = krpc::Error::method_unknown;
  const char *msg = "unknown method";
  krpc::response::error(ctx.out, ctx.transaction, e, msg);
  return true;
}

void
setup(dht::Module &module) noexcept {
  module.query = "";
  module.request = on_request;
  module.response = on_response;
}
} // namespace error
