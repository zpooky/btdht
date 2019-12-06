#include "dht_interface.h"
#include <inttypes.h>

#include "Log.h"
#include "bencode.h"
#include "bootstrap.h"
#include "client.h"
#include "db.h"
#include "dht.h"
#include "krpc.h"
#include "search.h"
#include "timeout.h"
#include "transaction.h"

#include <algorithm>
#include <collection/Array.h>
#include <cstring>
#include <prng/util.h>
#include <string/ascii.h>
#include <util/assert.h>
#include <utility>

namespace dht {
static Timeout
on_awake(DHT &dht, sp::Buffer &out) noexcept;

static Timeout
on_awake_ping(DHT &, sp::Buffer &) noexcept;

static Timeout
on_awake_bootstrap_reset(DHT &, sp::Buffer &) noexcept;

static Timeout
on_awake_eager_tx_timeout(DHT &, sp::Buffer &) noexcept;
} // namespace dht

//=====================================
namespace interface_dht {
bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  ping::setup(modules.module[i++]);
  find_node::setup(modules.module[i++]);
  get_peers::setup(modules.module[i++]);
  announce_peer::setup(modules.module[i++]);
  error::setup(modules.module[i++]);

  insert(modules.on_awake, &dht::on_awake);
  insert(modules.on_awake, &dht::on_awake_ping);
  insert(modules.on_awake, &db::on_awake_peer_db);
  insert(modules.on_awake, &dht::on_awake_bootstrap_reset);
  insert(modules.on_awake, &dht::on_awake_eager_tx_timeout);

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
  const std::uint8_t max = ~0;
  if (node.outstanding != max) {
    ++node.outstanding;
  }
}

/*pings*/
static Timeout
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
    const Timestamp next = tHead->req_sent + cfg.refresh_interval;
    ctx.timeout_next = next;

    if (next > ctx.now) {
      Timestamp next_seconds = next - ctx.now;
      // return std::max(config.min_timeout_interval, next_seconds);
      return cfg.min_timeout_interval > next_seconds
                 ? Timeout(cfg.min_timeout_interval)
                 : next_seconds;
    }

    return Timeout(cfg.min_timeout_interval);
  } else {
    /* timeout queue is empty */
    ctx.timeout_next = ctx.now + cfg.refresh_interval;
  }

  return cfg.refresh_interval;
}

static Timeout
on_awake_bootstrap_reset(DHT &self, sp::Buffer &) noexcept {
  Timestamp timeout = self.bootstrap_last_reset + self.config.bootstrap_reset;
  /* Only reset if there is a small amount of nodes in self.bootstrap since we
   * are starved for potential contacts
   */
  if (self.now >= timeout) {
    // XXX if_empty(bootstrap) try to fetch more nodes from dump.file
    if (is_empty(self.bootstrap) || nodes_good(self) < 100) {
      bootstrap_reset(self);
    }

    self.bootstrap_last_reset = self.now;
    timeout = self.now + self.config.bootstrap_reset;
  }

  return timeout - self.now;
}

static Timeout
on_awake_eager_tx_timeout(DHT &self, sp::Buffer &) noexcept {
  Config &cfg = self.config;
  const auto timeout = cfg.refresh_interval;

  tx::eager_tx_timeout(self, timeout);
  auto head = tx::next_timeout(self.client);
  if (!head) {
    return timeout;
  }

  auto result = std::max(head->sent + timeout, self.now) - self.now;
  assertx(result > sp::Milliseconds(0));
  return result;
}

static Timeout
awake_look_for_nodes(DHT &self, sp::Buffer &out, std::size_t missing_contacts) {
  std::size_t now_sent = 0;

  auto inc_active_searches = [&self, &missing_contacts, &now_sent]() {
    std::size_t K = dht::Bucket::K;
    missing_contacts -= std::min(missing_contacts, K);
    self.active_searches++;
    now_sent++;
  };

  Config &cfg = self.config;
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
    if (now_sent > 0) {
      auto next_timestamp = tx::next_available(self);
      return next_timestamp - self.now;
    } else {
      // arbritary?
      return sp::Timestamp(cfg.transaction_timeout);
    }
  }

  return cfg.refresh_interval;
} // awake_look_for_nodes()

static Timeout
on_awake(DHT &dht, sp::Buffer &out) noexcept {
  Timeout result(dht.config.refresh_interval);

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
    log::awake::contact_scan(dht);
  }

  return result;
}

static Node *
dht_activity(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  DHT &self = ctx.dht;

  Node *result = find_contact(self, sender);
  if (result) {
    if (!result->good) {
      result->good = true;
      result->outstanding = 0; // XXX
      assertx(self.bad_nodes > 0);
      self.bad_nodes--;
    }
  } else {
    Node contact(sender, ctx.remote, self.now);
    result = dht::insert(self, contact);
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

  if (dht::is_blacklisted(self, ctx.remote)) {
    return;
  }

  dht::Node *contact = dht_activity(ctx, sender);

  handle_ip_election(ctx, sender);
  if (contact) {
    contact->remote_activity = self.now;
    f(*contact);
  } else {
    bootstrap_insert(self, dht::KContact(sender.id, ctx.remote, self.id.id));
    dht::Node n;
    n.id = sender;
    f(n);
  }

  return;
}

static void
print_raw(FILE *f, const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    fprintf(f, "'%.*s': %zu", int(len), val, len);
  } else {
    fprintf(f, "hex[");
    dht::print_hex((const sp::byte *)val, len);
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
  log::receive::req::ping(ctx);

  message(ctx, sender, [&ctx](auto &) {
    dht::DHT &dht = ctx.dht;
    krpc::response::ping(ctx.out, ctx.transaction, dht.id);
  });
  // XXX response

  return true;
} // ping::handle_request()

static bool
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  log::receive::res::ping(ctx);

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

    log::receive::parse::error(ctx.dht, p, "'ping' response missing 'id'");
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

    log::receive::parse::error(ctx.dht, p, "'ping' request missing 'id'");
    return false;
  });
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
           void *arg) noexcept {
  log::transmit::error::ping_response_timeout(dht, tx, sent);
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
               const dht::NodeId &search) noexcept {
  log::receive::req::find_node(ctx);

  message(ctx, sender, [&](auto &) {
    dht::DHT &dht = ctx.dht;
    constexpr std::size_t capacity = 8;

    const krpc::Transaction &t = ctx.transaction;
    dht::Node *result[capacity] = {nullptr};
    dht::multiple_closest(dht, search, result);
    const dht::Node **r = (const dht::Node **)&result;

    krpc::response::find_node(ctx.out, t, dht.id, r, capacity);
  });
} // find_node::handle_request()

template <typename ContactType>
static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                /*const sp::list<dht::Node>*/ ContactType &contacts) noexcept {
  static_assert(
      std::is_same<dht::IdContact, typename ContactType::value_type>::value,
      "");
  log::receive::res::find_node(ctx);

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

  assertx(dht.active_searches > 0);
  dht.active_searches--;
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
           void *closure) noexcept {
  log::transmit::error::find_node_response_timeout(dht, tx, sent);
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
      log::receive::parse::error(ctx.dht, p, msg);
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

    dht::NodeId id;
    dht::NodeId target;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }
    if (!b_t && bencode::d::pair(p, "target", target.id)) {
      b_t = true;
      goto Lstart;
    }

    if (bencode_any(p, "find_node req")) {
      goto Lstart;
    }

    if (!(b_id && b_t)) {
      const char *msg = "'find_node' request missing 'id' or 'target'";
      log::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    handle_request(ctx, id, target);
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
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &id,
               const dht::Infohash &search) noexcept {
  log::receive::req::get_peers(ctx);

  message(ctx, id, [&](auto &from) {
    dht::Token token;
    dht::DHT &dht = ctx.dht;
    // TODO ip
    db::mint_token(dht, from, ctx.remote, token);

    const krpc::Transaction &t = ctx.transaction;
    const dht::KeyValue *result = db::lookup(dht, search);
    if (result) {
      krpc::response::get_peers(ctx.out, t, dht.id, token, result->peers);
    } else {
      constexpr std::size_t capacity = 8;
      dht::Node *closest[capacity] = {nullptr};
      dht::multiple_closest(dht, search, closest);
      const dht::Node **r = (const dht::Node **)&closest;

      krpc::response::get_peers(ctx.out, t, dht.id, token, r, capacity);
    }
  });
} // get_peers::handle_request

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

  log::receive::res::get_peers(ctx);
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
  log::transmit::error::get_peers_response_timeout(dht, tx, sent);
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
    log::receive::parse::error(ctx.dht, p, msg);
    return false;
  });
} // get_peers::on_response

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    bool b_id = false;
    bool b_ih = false;

    dht::NodeId id;
    dht::Infohash infohash;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (!b_ih && bencode::d::pair(p, "info_hash", infohash.id)) {
      b_ih = true;
      goto Lstart;
    }

    if (bencode_any(p, "get_peers req")) {
      goto Lstart;
    }

    if (!(b_id && b_ih)) {
      const char *msg = "'get_peers' request missing 'id' or 'info_hash'";
      log::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    handle_request(ctx, id, infohash);
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
// announce_peer
//===========================================================
namespace announce_peer {
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender,
               bool implied_port, const dht::Infohash &infohash, Port port,
               const dht::Token &token) noexcept {
  log::receive::req::announce_peer(ctx);

  dht::DHT &dht = ctx.dht;
  message(ctx, sender, [&](auto &from) {
    if (db::valid(dht, from, token)) {
      Contact peer(ctx.remote);
      if (implied_port || port == 0) {
        peer.port = ctx.remote.port;
      } else {
        peer.port = port;
      }

      db::insert(dht, infohash, peer);
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
  log::receive::res::announce_peer(ctx);

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
      log::receive::parse::error(ctx.dht, p, msg);
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

    dht::NodeId id;
    bool implied_port = false;
    dht::Infohash infohash;
    Port port = 0;
    dht::Token token;

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

    if (bencode_any(p, "announce_peer req")) {
      goto Lstart;
    }

    if (!(b_id && b_ih && b_p && b_t)) {
      const char *msg = "'announce_peer' request missing 'id' or 'info_hash' "
                        "or 'port' or 'token'";
      log::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    handle_request(ctx, id, implied_port, infohash, port, token);
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
// error
//===========================================================
namespace error {
static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  log::receive::res::error(ctx);

  return true;
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  log::receive::req::error(ctx);

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