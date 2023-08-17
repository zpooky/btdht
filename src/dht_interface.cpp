#include "dht_interface.h"
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
#include "search.h"
#include "timeout.h"
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
on_awake(DHT &dht, sp::Buffer &out) noexcept;

static Timestamp
on_awake_ping(DHT &, sp::Buffer &) noexcept;

static Timestamp
on_awake_bootstrap_reset(DHT &, sp::Buffer &) noexcept;

static Timestamp
on_awake_eager_tx_timeout(DHT &, sp::Buffer &) noexcept;
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
    insert(modules.awake.on_awake, &dht::on_awake);
    insert(modules.awake.on_awake, &dht::on_awake_ping);
    insert(modules.awake.on_awake, &db::on_awake_peer_db);
    insert(modules.awake.on_awake, &dht::on_awake_bootstrap_reset);
    insert(modules.awake.on_awake, &dht::on_awake_eager_tx_timeout);
  }

  return true;
}
} // namespace interface_dht

//=====================================

namespace timeout {
template <typename F>
bool
for_all_node(dht::DHT &self, sp::Milliseconds timeout, F f) {
  const dht::Node *start = nullptr;
  assertx(debug_assert_all(self));
  bool result = true;
Lstart : {
  dht::Node *const node = timeout::take_node(self, timeout);
  if (node) {
    if (node == start) {
      assertx(!timeout::debug_find_node(self, node));
      timeout::prepend(self, node);
      assertx(node->timeout_next);
      assertx(node->timeout_priv);
      assertx(timeout::debug_find_node(self, node) == node);
      assertx(debug_assert_all(self));
      return true;
    }

    if (!start) {
      start = node;
    }
    assertx(!timeout::debug_find_node(self, node));

    // printf("node: %s\n", to_hex(node->id));

    assertx(!node->timeout_next);
    assertx(!node->timeout_priv);

    if (node->good) {
      if (dht::should_mark_bad(self, *node)) { // TODO ??
        node->good = false;
        self.bad_nodes++;
      }
    }

    if (f(self, *node)) {
      timeout::append_all(self, node);
      assertx(node->timeout_next);
      assertx(node->timeout_priv);
      assertx(timeout::debug_find_node(self, node) == node);
      assertx(debug_assert_all(self));

      goto Lstart;
    } else {
      timeout::prepend(self, node);
      assertx(node->timeout_next);
      assertx(node->timeout_priv);
      assertx(timeout::debug_find_node(self, node) == node);
      assertx(debug_assert_all(self));

      result = false;
    }
  }
}

  return result;
}
} // namespace timeout

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
  const std::uint8_t max(~0);
  if (node.outstanding != max) {
    ++node.outstanding;
  }
}

/*pings*/
static Timestamp
on_awake_ping(DHT &ctx, sp::Buffer &out) noexcept {
  Config &cfg = ctx.config;

  /* Send ping to nodes */
  auto f = [&out](auto &dht, auto &node) {
    bool result = client::ping(dht, out, node) == client::Res::OK;
    if (result) {
      inc_outstanding(node);
      // timeout::update_send(dht, node);

      /* Fake update activity otherwise if all nodes have to
       * same timeout we will spam out pings, ex: 3 nodes timed
       * out, send ping, append, get the next timeout date,
       * since there is only 3 in the queue and we will
       * immediately awake and send ping  to the same 3 nodes
       */
      node.req_sent = dht.now;
    }

    return result;
  };
  timeout::for_all_node(ctx, cfg.refresh_interval, f);

  /* Calculate next timeout based on the head if the timeout list which is in
   * sorted order where to oldest node is first in the list.
   */
  Node *const tHead = ctx.timeout_node;
  if (tHead) {
    ctx.timeout_next = tHead->req_sent + cfg.refresh_interval;

    if (ctx.now >= ctx.timeout_next) {
      ctx.timeout_next = ctx.now + cfg.min_timeout_interval;
    }

  } else {
    /* timeout queue is empty */
    ctx.timeout_next = ctx.now + cfg.refresh_interval;
  }

  assertx(ctx.timeout_next > ctx.now);
  return ctx.timeout_next;
}

static Timestamp
on_awake_bootstrap_reset(DHT &self, sp::Buffer &) noexcept {
  Config &cfg = self.config;
  Timestamp next = self.bootstrap_last_reset + cfg.bootstrap_reset;
  /* Only reset if there is a small amount of nodes in self.bootstrap since we
   * are starved for potential contacts
   */
  if (self.now >= next) {
    // XXX if_empty(bootstrap) try to fetch more nodes from dump.file
    if (is_empty(self.bootstrap) || nodes_good(self) < 100) {
      bootstrap_reset(self);
    }

    self.bootstrap_last_reset = self.now;
    next = self.bootstrap_last_reset + cfg.bootstrap_reset;
  }

  return next;
}

static Timestamp
on_awake_eager_tx_timeout(DHT &self, sp::Buffer &) noexcept {
  Config &cfg = self.config;
  const auto timeout = cfg.refresh_interval;

  tx::eager_tx_timeout(self, timeout);
  auto head = tx::next_timeout(self.client);
  if (!head) {
    return self.now + timeout;
  }

  assertxs((head->sent + timeout) > self.now, uint64_t(head->sent),
           uint64_t(timeout), uint64_t(self.now));
  return head->sent + timeout;
}

static Timestamp
awake_look_for_nodes(DHT &self, sp::Buffer &out, std::size_t missing_contacts) {
  Config &cfg = self.config;
  std::size_t now_sent = 0;

  auto inc_active_searches = [&self, &missing_contacts, &now_sent]() {
    std::size_t K = dht::Bucket::K;
    missing_contacts -= std::min(missing_contacts, K);
    self.active_find_nodes++;
    now_sent++;
  };

  // XXX self should not be in bootstrap list
  // XXX if no good node is available try bad/questionable nodes

  auto result = client::Res::OK;

  auto f = [&](auto &ctx, Node &remote) {
    // if (dht::is_good(ctx, remote)) {
    const Contact &c = remote.contact;
    dht::NodeId &sid = self.id;

    result = client::find_node(ctx, out, c, /*search*/ sid, nullptr);
    if (result == client::Res::OK) {
      inc_outstanding(remote);
      inc_active_searches();
      remote.req_sent = self.now;
    }
    // }

    return result == client::Res::OK;
  };
  timeout::for_all_node(self, cfg.refresh_interval, f);

  if (result != client::Res::ERR_TOKEN) {
    /* Bootstrap contacts */
    dht::KContact cur;
    while (bootstrap_take_head(self, cur)) {
      auto closure = bootstrap_alloc(self, cur);
      result = client::find_node(self, out, cur.contact, self.id, closure);
      if (result == client::Res::OK) {
        inc_active_searches();
      } else {
        insert_eager(self.bootstrap, cur);
        bootstrap_reclaim(self, closure);
        break;
      }
    } // while
  }

  if (missing_contacts > 0) {
    Timestamp next = tx::next_available(self);
    if (next > self.now) {
      return next;
    } else {
      // arbritary?
      return self.now + cfg.transaction_timeout;
    }
  }

  return self.now + cfg.refresh_interval;
} // awake_look_for_nodes()

static Timestamp
on_awake(DHT &dht, sp::Buffer &out) noexcept {
  Timestamp result(dht.now + dht.config.refresh_interval);

  auto percentage = [](std::uint32_t t, std::uint32_t c) {
    return double(c) / double(t / .100);
  };

  auto good = nodes_good(dht);
  const uint32_t all = dht::max_routing_nodes(dht);
  uint32_t look_for = all - good;

  const auto cur = percentage(all, good);
  printf("good[%u], total[%u], bad[%u], look_for[%u], "
         "config.seek[%zu%s], "
         "cur[%.2f%s], max[%u], dht.root[%zd], bootstraps[%zu]\n",
         good, nodes_total(dht), nodes_bad(dht), look_for, //
         dht.config.percentage_seek, "%",                  //
         cur, "%", all, dht.root ? dht.root->depth : 0, length(dht.bootstrap));

  if (cur < (double)dht.config.percentage_seek) {
    // TODO if we can't mint new tx then next should be calculated base on when
    // soonest next tx timesout is so we can directly reuse it. (it should not
    // be the config.refresh_interval of 15min if we are under conf.p_seek)
    auto awake_next = awake_look_for_nodes(dht, out, look_for);
    result = std::min(result, awake_next);
    logger::awake::contact_scan(dht);
  }

  assertxs(result > dht.now, uint64_t(result), uint64_t(dht.now));
  return result;
}

static Node *
dht_activity(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  DHT &self = ctx.dht;

  Node *result = find_contact(self, sender);
  if (result) {
    result->read_only = ctx.read_only;
    if (!result->good) {
      result->good = true;
      result->outstanding = 0; // XXX
      assertx(self.bad_nodes > 0);
      self.bad_nodes--;
    }
  } else {
    if (!ctx.read_only) {
      Node contact(sender, ctx.remote, self.now);
      result = dht::insert(self, contact);
    }
  }

  return result;
}

} // namespace dht

static void
handle_ip_election(dht::MessageContext &ctx, const dht::NodeId &) noexcept {
  // TODO?
  if (bool(ctx.ip_vote)) {
    // if (is_strict(ctx.remote.ip, sender)) {
    const Contact &v = ctx.ip_vote.get();
    auto &dht = ctx.dht;
    vote(dht.election, ctx.remote, v);
    // } else {
    //   // assertx(false);
    // }
  }
}

template <typename F>
static bool
message(dht::MessageContext &ctx, const dht::NodeId &sender, F f) noexcept {
  dht::DHT &self = ctx.dht;

  if (!dht::is_valid(sender)) {
    logger::receive::parse::invalid_node_id(ctx, sender);
    return false;
  }

  /*request from self*/
  if (self.id == sender.id) {
    logger::receive::parse::self_sender(ctx);
    return false;
  }

  if (ctx.domain == dht::Domain::Domain_private) {
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
        bootstrap_insert(self,
                         dht::KContact(sender.id, ctx.remote, self.id.id));
      }
      dht::Node n{sender, ctx.remote};
      f(n);
    }
  }

  return true;
}

static void
print_raw(FILE *f, const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    fprintf(f, "'%.*s': %zu", int(len), val, len);
  } else {
    fprintf(f, "hex[");
    dht::print_hex(f, (const sp::byte *)val, len);
    fprintf(f, "](");
    for (std::size_t i = 0; i < len; ++i) {
      if (ascii::is_printable(val[i])) {
        fprintf(f, "%c", val[i]);
      } else {
        fprintf(f, "_");
      }
    }
    fprintf(f, ")");
  }
}

static bool
bencode_any(sp::Buffer &p, const char *ctx) noexcept {
  FILE *f = stderr;
  /*any str*/ {
    const char *kit = nullptr;
    std::size_t klen = 0;

    const unsigned char *vit = nullptr;
    std::size_t vlen = 0;

    if (bencode::d::pair_ref(p, kit, klen, vit, vlen)) {
      fprintf(f, "%s str[", ctx);
      print_raw(f, kit, klen);
      fprintf(f, ", ");
      print_raw(f, (const char *)vit, vlen);
      fprintf(f, "] \n");
      return true;
    }
  }

  /*any int*/ {
    const char *kit = nullptr;
    std::size_t klen = 0;

    std::uint64_t value = 0;

    if (bencode::d::pair_ref(p, kit, klen, value)) {
      fprintf(f, "%s int[", ctx);
      print_raw(f, kit, klen);
      fprintf(f, ", %" PRIu64 "]\n", value);
      return true;
    }
  }

  /*any list*/ {
    const size_t pos = p.pos;
    const char *kit = nullptr;
    std::size_t klen = 0;

    if (bencode::d::value_ref(p, kit, klen)) {
      bool first = true;

      auto cb = [&](sp::Buffer &p2) { //
        fprintf(f, "%s list[", ctx);
        print_raw(f, kit, klen);
        fprintf(f, "\n");
        first = false;

        while (true) {
          const char *vit = nullptr;
          std::size_t vlen = 0;
          std::uint64_t value = 0;

          if (bencode::d::value_ref(p2, vit, vlen)) {
            fprintf(f, "- ");
            print_raw(f, (const char *)vit, vlen);
            fprintf(f, "\n");
          } else if (bencode::d::value(p2, value)) {
            fprintf(f, "- ");
            fprintf(f, ", %" PRIu64 "]\n", value);
          } else {
            break;
          }
        }

        return true;
      };

      if (bencode::d::list(p, cb)) {
        fprintf(f, "]\n");
        return true;
      }

      fprintf(f, "spooky[error]\n");
      p.pos = pos;
    }
  }

  return false;
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
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
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
      dht::multiple_closest(dht, search, result);
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
    for_each(contacts, [&](const auto &contact) {
      dht::DHT &dht = ctx.dht;

      bootstrap_insert(dht, contact);
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

  assertx(dht.active_find_nodes > 0);
  dht.active_find_nodes--;
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
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
        if (nodes_good(dht) < 100) {
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
    return handle_request(ctx, req.id, req.target, req.n4, req.n6);
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
    db::mint_token(dht, ctx.remote, token);

    const krpc::Transaction &t = ctx.transaction;

    const dht::KeyValue *const result = db::lookup(dht, search);
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
      dht::multiple_closest(dht, search, closest);
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
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &, // XXX store token to used for announce
                const ResultType /*sp::list<Contact>*/ &result,
                const ContactType /*sp::list<dht::Node>*/ &contacts,
                dht::Search &search) noexcept {
  static_assert(std::is_same<Contact, typename ResultType::value_type>::value,
                "");
  static_assert(
      std::is_same<dht::IdContact, typename ContactType::value_type>::value,
      "");

  logger::receive::res::get_peers(ctx);
  dht::DHT &dht = ctx.dht;

  message(ctx, sender, [&](auto &) {
    /* Sender returns the its closest contacts for search */
    for_each(contacts, [&](const auto &contact) {
      search_insert(search, contact);

      bootstrap_insert(dht, contact);
    });

    /* Sender returns matching values for search query */
    for_each(result, [&search](const Contact &c) {
      /**/
      search_insert_result(search, c);
    });
  });

  return true;
} // namespace get_peers

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
           void *ctx) noexcept {
  logger::transmit::error::get_peers_response_timeout(dht, tx, sent);
  auto search = (dht::SearchContext *)ctx;
  if (search) {
    search_decrement(search);
  }
} // get_peers::on_timeout

static bool
on_response(dht::MessageContext &ctx, void *searchCtx) noexcept {
  krpc::GetPeersResponse res;
  // assertx(searchCtx);
  if (!searchCtx) {
    return true;
  }

  auto search_ctx = (dht::SearchContext *)searchCtx;
  dht::Search *search = search_find(ctx.dht, search_ctx);
  search_decrement(search_ctx);

  // assertx(search);
  if (!search) {
    return true;
  }

  if (krpc::parse_get_peers_response(ctx, res)) {
    return handle_response(ctx, res.id, res.token, res.values, res.nodes,
                           *search);
  }

  return false;
} // get_peers::on_response

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::GetPeersRequest req;
  if (krpc::parse_get_peers_request(ctx, req)) {
    return handle_get_peers_request(ctx, req.id, req.infohash, req.noseed,
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
    if (db::is_valid_token(dht, from, token)) {
      Contact peer(ctx.remote);
      if (implied_port || port == 0) {
        peer.port = ctx.remote.port;
      } else {
        peer.port = port;
      }

      db::insert(dht, infohash, peer, seed, name);
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

    return handle_announce_peer_request(ctx, req.id, req.implied_port,
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
// sample_infohashes
//===========================================================
namespace sample_infohashes {
static bool
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender,
               const dht::Infohash &ih) noexcept {
  logger::receive::req::sample_infohashes(ctx);

  message(ctx, sender, [&ctx, &ih](auto &) {
    dht::DHT &self = ctx.dht;
    constexpr std::size_t capacity = 8;

    dht::Node *nodes[capacity] = {nullptr};
    dht::multiple_closest(self, ih, nodes);

    std::uint32_t num = self.db.length_lookup_table;
    sp::UinStaticArray<dht::Infohash, 20> &samples =
        db::randomize_samples(self);
    std::uint32_t interval = db::next_randomize_samples(self);

    krpc::response::sample_infohashes(ctx.out, ctx.transaction, self.id,
                                      interval, (const dht::Node **)nodes,
                                      capacity, num, samples);
  });
  // XXX response

  return true;
}

// static bool
// handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept
// {
//   return true;
// }

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  //   return bencode::d::dict(ctx.in, [&ctx](auto &p) {
  //     bool b_id = false;
  //     bool b_ip = false;
  //
  //     dht::NodeId sender;
  //
  //   Lstart:
  //     if (!b_id && bencode::d::pair(p, "id", sender.id)) {
  //       b_id = true;
  //       goto Lstart;
  //     }
  //
  //     {
  //       Contact ip;
  //       if (!b_ip && bencode::d::pair(p, "ip", ip)) {
  //         ctx.ip_vote = ip;
  //         assertx(bool(ctx.ip_vote));
  //         b_ip = true;
  //         goto Lstart;
  //       }
  //     }
  //
  //     if (bencode_any(p, "ping resp")) {
  //       goto Lstart;
  //     }
  //
  //     if (b_id) {
  //       return handle_response(ctx, sender);
  //     }
  //
  // logger::receive::parse::error(ctx.dht, p, "'ping' response missing 'id'");
  //     return false;
  // });
  return true;
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  krpc::SampleInfohashesRequest req;
  if (krpc::parse_sample_infohashes_request(ctx, req)) {
    return handle_request(ctx, req.sender, req.ih);
  }
  return false;
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
           void *arg) noexcept {
  logger::transmit::error::sample_infohashes_response_timeout(dht, tx, sent);
  assertx(arg == nullptr);
}

bool
setup(dht::Module &m) noexcept {
  m.query = "sample_infohashes";
  m.request = on_request;
  m.response = on_response;
  m.response_timeout = on_timeout;
  return true;
}
} // namespace sample_infohashes

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
