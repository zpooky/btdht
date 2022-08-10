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

  auto percentage = [](std::uint32_t t, std::uint32_t c) -> std::size_t {
    return std::size_t(c) / std::size_t(t / std::size_t(100));
  };

  auto good = nodes_good(dht);
  const uint32_t all = dht::max_routing_nodes(dht);
  uint32_t look_for = all - good;

  const auto cur = percentage(all, good);
  printf("good[%u], total[%u], bad[%u], look_for[%u], "
         "config.seek[%zu%s], "
         "cur[%zu%s], max[%u], dht.root[%zd], bootstraps[%zu]\n",
         good, nodes_total(dht), nodes_bad(dht), look_for, //
         dht.config.percentage_seek, "%",                  //
         cur, "%", all, dht.root ? dht.root->depth : 0, length(dht.bootstrap));

  if (cur < dht.config.percentage_seek) {
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
static void
message(dht::MessageContext &ctx, const dht::NodeId &sender, F f) noexcept {
  dht::DHT &self = ctx.dht;

  if (!dht::is_valid(sender)) {
    return;
  }

  /*request from self*/
  if (self.id == sender.id) {
    return;
  }

  if (ctx.domain == dht::Domain::Domain_private) {
    dht::Node dummy{};
    f(dummy);
  } else {
    assertx(ctx.remote.port != 0);
    if (dht::is_blacklisted(self, ctx.remote)) {
      return;
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

  return;
}

static void
print_raw(FILE *f, const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    fprintf(f, "'%.*s': %zu", int(len), val, len);
  } else {
    fprintf(f, "hex[");
    dht::print_hex((const sp::byte *)val, len, f);
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
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    bool b_id = false;
    bool b_ip = false;

    dht::NodeId sender;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", sender.id)) {
      b_id = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (bencode_any(p, "ping resp")) {
      goto Lstart;
    }

    if (b_id) {
      return handle_response(ctx, sender);
    }

    logger::receive::parse::error(ctx.dht, p, "'ping' response missing 'id'");
    return false;
  });
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    bool b_id = false;
    bool b_ip = false;

    dht::NodeId sender;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", sender.id)) {
      b_id = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (b_id) {
      return handle_request(ctx, sender);
    }

    logger::receive::parse::error(ctx.dht, p, "'ping' request missing 'id'");
    return false;
  });
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
static void
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
} // find_node::handle_request()

template <typename ContactType>
static void
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

  return bencode::d::dict(ctx.in, [&ctx, &dht, cap_ptr](auto &p) { //
    bool b_id = false;
    bool b_n = false;
    bool b_p = false;
    bool b_ip = false;
    bool b_t = false;

    dht::NodeId id;
    dht::Token token;

    auto &nodes = dht.recycle_id_contact_list;
    clear(nodes);

    std::uint64_t p_param = 0;

  Lstart:
    const std::size_t pos = p.pos;
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    } else {
      assertx(p.pos == pos);
    }

    // optional
    if (!b_n) {
      clear(nodes);
      // TODO we parse a node which get 0 as port
      // - ipv4 = 1148492139,
      if (bencode::d::nodes(p, "nodes", nodes)) {
        b_n = true;
        goto Lstart;
      } else {
        assertx(p.pos == pos);
      }
    }

    if (!b_t && bencode::d::pair(p, "token", token)) {
      b_t = true;
      goto Lstart;
    } else {
      assertx(p.pos == pos);
    }

    // optional
    if (!b_p && bencode::d::pair(p, "p", p_param)) {
      b_p = true;
      goto Lstart;
    } else {
      assertx(p.pos == pos);
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      } else {
        assertx(p.pos == pos);
      }
    }

    if (bencode_any(p, "find_node resp")) {
      goto Lstart;
    }

    if (!(b_id)) {
      const char *msg = "'find_node' response missing 'id'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    if (is_empty(nodes)) {
      if (cap_ptr) {
        if (nodes_good(dht) < 100) {
          /* Only remove bootstrap node if we have gotten some nodes from it */
          bootstrap_insert_force(dht, *cap_ptr);
        }
      }
    }

    handle_response(ctx, id, nodes);
    return true;
  });
} // find_node::on_response

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_want = false;

    dht::NodeId id;
    dht::NodeId target;

    sp::UinStaticArray<std::string, 2> want;
    bool n4 = false;
    bool n6 = false;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }
    if (!b_t && bencode::d::pair(p, "target", target.id)) {
      b_t = true;
      goto Lstart;
    }

    if (!b_want && bencode_d<sp::Buffer>::pair(p, "want", want)) {
      b_want = true;
      for (std::string &w : want) {
        if (w == "n4") {
          n4 = true;
        } else if (w == "n6") {
          n6 = true;
        }
      }
      goto Lstart;
    }

    if (bencode_any(p, "find_node req")) {
      goto Lstart;
    }

    if (!(b_id && b_t)) {
      const char *msg = "'find_node' request missing 'id' or 'target'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    if (!b_want) {
      n4 = true;
    }

    handle_request(ctx, id, target, n4, n6);
    return true;
  });
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
// TODO test
template <int SIZE>
static void
bloomfilter_insert(uint8_t (&bloom)[SIZE], const Ip &ip) noexcept {
  const int m = SIZE * 8;
  uint8_t hash[20]{0};
  SHA1_CTX ctx;

  SHA1Init(&ctx);

  if (ip.type == IpType::IPV4) {
    const void *raw = (const void *)&ip.ipv4;
    SHA1Update(&ctx, (const uint8_t *)raw, (uint32_t)sizeof(ip.ipv4));
  } else {
    const void *raw = (const void *)&ip.ipv6.raw;
    SHA1Update(&ctx, (const uint8_t *)raw, (uint32_t)sizeof(ip.ipv6));
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

static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &id,
               const dht::Infohash &search, sp::maybe<bool> mnoseed,
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

        krpc::response::get_peers_scrape(ctx.out, t, dht.id, token, peers,
                                         seeds);
        return;
      }
    } else {
      if (result) {
        clear(dht.recycle_contact_list);
        auto &filtered = dht.recycle_contact_list;
        for_each(result->peers, [&filtered, &mnoseed](const dht::Peer &cur) {
          if (mnoseed) {
            if ((cur.seed == false && mnoseed.get() == true) ||
                (cur.seed == true && mnoseed.get() == false)) {
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
} // namespace get_peers

template <typename ResultType, typename ContactType>
static void
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

  return bencode::d::dict(ctx.in, [&ctx, search](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_n = false;
    bool b_v = false;
    // bool b_ip = false;

    dht::NodeId id;
    dht::Token token;

    dht::DHT &dht = ctx.dht;
    auto &nodes = dht.recycle_id_contact_list;
    clear(nodes);

    auto &values = dht.recycle_contact_list;
    clear(values);

  Lstart:
    const std::size_t pos = p.pos;
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    } else {
      assertx(pos == p.pos);
    }

    if (!b_t && bencode::d::pair(p, "token", token)) {
      b_t = true;
      goto Lstart;
    } else {
      assertx(pos == p.pos);
    }

    // XXX
    // {
    //   Contact ip;
    //   if (!b_ip && bencode::d::pair(p, "ip", ip)) {
    //     ctx.ip_vote = ip;
    //     assertx(bool(ctx.ip_vote));
    //     b_ip = true;
    //     goto Lstart;
    //   } else {
    //     assertx(pos == p.pos);
    //   }
    // }

    /*closes K nodes*/
    if (!b_n) {
      clear(nodes);
      if (bencode::d::nodes(p, "nodes", nodes)) {
        b_n = true;
        goto Lstart;
      } else {
        assertx(pos == p.pos);
      }
    }

    if (!b_v) {
      clear(values);
      if (bencode::d::peers(p, "values", values)) {
        b_v = true;
        goto Lstart;
      } else {
        assertx(pos == p.pos);
      }
    }

    if (bencode_any(p, "get_peers resp")) {
      goto Lstart;
    }

    if (b_id && b_t && (b_n || b_v)) {
      handle_response(ctx, id, token, values, nodes, *search);
      return true;
    }

    const char *msg = "'get_peers' response missing 'id' and 'token' or "
                      "('nodes' or 'values')";
    logger::receive::parse::error(ctx.dht, p, msg);
    return false;
  });
} // get_peers::on_response

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    bool b_id = false;
    bool b_ih = false;
    bool b_ns = false;
    bool b_sc = false;
    bool b_want = false;

    dht::NodeId id;
    dht::Infohash infohash;
    sp::maybe<bool> noseed{};
    bool scrape = false;

    sp::UinStaticArray<std::string, 2> want;
    bool n4 = false;
    bool n6 = false;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (!b_ih && bencode::d::pair(p, "info_hash", infohash.id)) {
      b_ih = true;
      goto Lstart;
    }

    bool tmp_noseed = false;
    if (!b_ns && bencode::d::pair(p, "noseed", tmp_noseed)) {
      b_ns = true;
      noseed = tmp_noseed;
      goto Lstart;
    }

    if (!b_sc && bencode::d::pair(p, "scrape", scrape)) {
      b_sc = true;
      goto Lstart;
    }

    if (!b_want && bencode_d<sp::Buffer>::pair(p, "want", want)) {
      b_want = true;
      for (std::string &w : want) {
        if (w == "n4") {
          n4 = true;
        } else if (w == "n6") {
          n6 = true;
        }
      }
      goto Lstart;
    }

    if (bencode_any(p, "get_peers req")) {
      goto Lstart;
    }

    if (!(b_id && b_ih)) {
      const char *msg = "'get_peers' request missing 'id' or 'info_hash'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    if (!b_want) {
      // default:
      n4 = true;
    }

    handle_request(ctx, id, infohash, noseed, scrape, n4, n6);
    return true;
  });
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
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender,
               bool implied_port, const dht::Infohash &infohash, Port port,
               const dht::Token &token, bool seed, const char *name) noexcept {
  logger::receive::req::announce_peer(ctx);

  dht::DHT &dht = ctx.dht;
  message(ctx, sender, [&](auto &from) {
    if (db::valid(dht, from, token)) {
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
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  logger::receive::res::announce_peer(ctx);

  message(ctx, sender, [](auto &) { //

  });
}

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::NodeId id;
    bool b_id = false;

  Lstart:
    if (bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (bencode_any(p, "announce_peer resp")) {
      goto Lstart;
    }

    if (!(b_id)) {
      const char *msg = "'announce_peer' response missing 'id'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    handle_response(ctx, id);
    return true;
  });
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    bool b_id = false;
    bool b_ip = false;
    bool b_ih = false;
    bool b_p = false;
    bool b_t = false;
    bool b_s = false;
    bool b_n = false;

    dht::NodeId id;
    bool implied_port = false;
    dht::Infohash infohash;
    Port port = 0;
    dht::Token token;
    // According to BEP-33 if "seed" is omitted we assume it is a peer not a
    // seed
    bool seed = false;

    const char *name = nullptr;
    size_t name_len = 0;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }
    // optional
    if (!b_ip && bencode::d::pair(p, "implied_port", implied_port)) {
      b_ip = true;
      goto Lstart;
    }
    if (!b_ih && bencode::d::pair(p, "info_hash", infohash.id)) {
      b_ih = true;
      goto Lstart;
    }
    if (!b_p && bencode::d::pair(p, "port", port)) {
      b_p = true;
      goto Lstart;
    }
    if (!b_t && bencode::d::pair(p, "token", token)) {
      b_t = true;
      goto Lstart;
    }
    if (!b_s && bencode::d::pair(p, "seed", seed)) {
      b_s = true;
      goto Lstart;
    }

    if (!b_n && bencode::d::pair_value_ref(p, "name", name, name_len)) {
      b_n = true;
      goto Lstart;
    }

    if (bencode_any(p, "announce_peer req")) {
      goto Lstart;
    }

    if (!(b_id && b_ih && b_t)) {
      const char *msg =
          "'announce_peer' request missing 'id' or 'info_hash' or 'token'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    char name_abr[128]{'\0'};
    if (name) {
      memcpy(name_abr, name, std::min(name_len, sizeof(name_abr)));
      name_abr[127] = '\0';
    }

    handle_request(ctx, id, implied_port, infohash, port, token, seed,
                   name_abr);
    return true;
  });
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
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    bool b_id = false;
    bool b_target = false;

    dht::NodeId sender;
    dht::Infohash ih;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", sender.id)) {
      b_id = true;
      goto Lstart;
    }

    if (!b_target && bencode::d::pair(p, "target", ih.id)) {
      b_target = true;
      goto Lstart;
    }

    if (bencode_any(p, "sample_infohashes req")) {
      goto Lstart;
    }

    if (b_id) {
      return handle_request(ctx, sender, ih);
    }

    // logger::receive::parse::error(ctx.dht, p, "'ping' request missing 'id'");
    return false;
  });
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
