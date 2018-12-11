#include "dht_interface.h"

#include "Log.h"
#include "bencode.h"
#include "client.h"
#include "db.h"
#include "dht.h"
#include "krpc.h"
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
on_awake_peer_db(DHT &, sp::Buffer &) noexcept;
} // namespace dht

namespace interface_dht {

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  ping::setup(modules.module[i++]);
  find_node::setup(modules.module[i++]);
  get_peers::setup(modules.module[i++]);
  announce_peer::setup(modules.module[i++]);
  error::setup(modules.module[i++]);
  // TODO binary-insert with prio
  // 1. search
  // 2. find_node
  // 3. ping
  // 4. bookkeep peer db

  insert(modules.on_awake, &dht::on_awake);
  insert(modules.on_awake, &dht::on_awake_ping);
  insert(modules.on_awake, &dht::on_awake_peer_db);

  return true;
}

} // namespace interface_dht

namespace dht {

template <typename F>
static void
for_each(Node *node, F f) noexcept {
Lstart:
  if (node) {
    f(node);
    node = node->timeout_next;
    goto Lstart;
  }
}

static void
inc_outstanding(Node &node) noexcept {
  const std::uint8_t max = ~0;
  if (node.ping_outstanding != max) {
    ++node.ping_outstanding;
  }
}

} // namespace dht

namespace timeout {

template <typename T>
static T *
take(Timestamp now, sp::Milliseconds timeout, T *&the_head,
     std::size_t max) noexcept {
  auto is_expired = [now, timeout](auto &node) { //
    return (node.req_sent + timeout) > now;
  };

  T *result = nullptr;
  T *const head = the_head;
  T *current = head;
  std::size_t cnt = 0;

Lstart:
  if (current && cnt < max) {

    if (is_expired(*current)) {
      T *const next = current->timeout_next;
      timeout::unlink(the_head, current);

      if (!result) {
        result = current;
      } else {
        current->timeout_next = result;
        result = current;
      }
      ++cnt;

      if (next != head) {
        current = next;
        goto Lstart;
      } else {
        the_head = nullptr;
      }
    }
  }

  return result;
} // timeout::take()

// TODO XXX what is timeout????!?!?! (i want last sent timeout)

template <typename F>
bool
for_all(dht::DHT &ctx, sp::Milliseconds timeout, F f) {
// TODO make ping have a timeout and find_node has another
Lstart:

  dht::Node *node = timeout::take(ctx.now, timeout, ctx.timeout_node, 1);
  if (node) {
    assertx(node->timeout_next == nullptr);
    assertx(node->timeout_priv == nullptr);

    if (node->good) {
      if (dht::should_mark_bad(ctx, *node)) {
        node->good = false;
        ctx.bad_nodes++;
      }
    }

    if (f(ctx, *node)) {
      timeout::append_all(ctx, node);
      goto Lstart;
    } else {

      timeout::prepend(ctx, node);
      return false;
    }
  }

  return true;
}

static dht::Node *
head(dht::DHT &ctx) noexcept {
  return ctx.timeout_node;
} // timeout::head()

static void
update_receive(dht::DHT &, dht::Node *) noexcept {
  // TODO rename response_activity to receive_activity?
  // node->response_activity = dht.now;
  // return update(ctx, contact, now);
} // timeout::update()

} // namespace timeout

namespace dht {

/*pings*/
static Timeout
on_awake_ping(DHT &ctx, sp::Buffer &out) noexcept {
  Config &cfg = ctx.config;

  /* Send ping to nodes */
  timeout::for_all(ctx, cfg.refresh_interval, [&out](auto &dht, auto &node) {
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
      assertx(node.timeout_next == nullptr);
    }

    return result;
  });

  /* Calculate next timeout based on the head if the timeout list which is in
   * sorted order where to oldest node is first in the list.
   */
  Node *const tHead = timeout::head(ctx);
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
on_awake_peer_db(DHT &dht, sp::Buffer &) noexcept {
  // {
  // Lstart:
  //   Peer *const peer = timeout::take(dht.now, dht.timeout_peer, 1);
  //   if (peer) {
  //     assertx(peer->timeout_next == nullptr);
  //     assertx(peer->timeout_priv == nullptr);
  //   }
  // }
  // TODO

  return dht.config.refresh_interval;
}

static Timeout
awake_look_for_nodes(DHT &dht, sp::Buffer &out, std::size_t missing_contacts) {
  std::size_t now_sent = 0;

  auto inc_active_searches = [&dht, &missing_contacts, &now_sent]() {
    std::size_t K = dht::Bucket::K;
    missing_contacts -= std::min(missing_contacts, K);
    dht.active_searches++;
    now_sent++;
  };

  Config &cfg = dht.config;
  bool bs_sent = false;
  // TODO verify bad_nodes works...
  // TODO bootstrap should be last. Tag bucket with bootstrap generation only
  // use bootstrap if we get the same bucket and we haven't sent any to
  // bootstrap this generation.

  // XXX self should not be in bootstrap list
  // XXX how to handle the same bucket will be reselected to send find_nodes
  // multiple times in a row

  // TODO How to avoid flooding the same nodes with request especially when we
  // only have a few nodes in routing table?

  // XXX if no good node is available try bad/questionable nodes

  while (missing_contacts > 0) {
    auto result = client::Res::OK;
    std::size_t sent_count = 0;

    timeout::for_all(dht, cfg.refresh_interval, [&](auto &ctx, Node &remote) {
      if (dht::is_good(ctx, remote)) {
        const Contact &c = remote.contact;
        dht::NodeId &self = dht.id;

        result = client::find_node(ctx, out, c, /*search*/ self, nullptr);
        if (result == client::Res::OK) {
          ++sent_count;
          inc_active_searches();
          remote.req_sent = dht.now;
        }
      }

      return result == client::Res::OK;
    });

    if (result == client::Res::ERR_TOKEN) {
      break;
    }

    /* Bootstrap contacts */
    if (!bs_sent) {
      auto &bcontacts = dht.bootstrap_contacts;
      // TODO prune non good bootstrap nodes
      // XXX shuffle bootstrap list just before sending
      auto con = bcontacts.header[0];
      while (con) {
        dht::NodeId &self = dht.id;

        auto *closure = new (std::nothrow) Contact(con->value);
        result =
            client::find_node(dht, out, con->value, /*search*/ self, closure);
        if (result == client::Res::OK) {
          inc_active_searches();
          assertx_n(remove(bcontacts, con->value));
          con = bcontacts.header[0];
        } else {
          con = nullptr;
        }
      }
#if 0
      for_all(bcontacts, [&](const Contact &bs) {
        dht::NodeId &self = dht.id;

        auto *closure = new (std::nothrow) Contact(bs);
        result = client::find_node(dht, out, bs, /*search*/ self, closure);
        if (result == client::Res::OK) {
          inc_active_searches();
        }

        return result == client::Res::OK;
      });
#endif

      bs_sent = true;
    }

    if (result == client::Res::ERR_TOKEN) {
      break;
    }

    if (sent_count == 0) {
      break;
    }
  } // while missing_contacts > 0

  if (missing_contacts > 0) {
    if (now_sent > 0) {
      auto next_timestamp = tx::next_available(dht);
      return next_timestamp - dht.now;
    } else {
      // arbritary?
      return sp::Timestamp(cfg.transaction_timeout);
    }
  } else {
    // TODO only for debug
    assertxs(false, nodes_good(dht), nodes_total(dht), nodes_bad(dht));
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
  const auto all = dht::max_routing_nodes(dht);
  auto look_for = all - good;

  const auto cur = percentage(all, good);
  printf("good[%u], total[%u], bad[%u], look_for[%u], "
         "config.seek[%zu%s], "
         "cur[%zu%s], max[%u]\n",
         good, nodes_total(dht), nodes_bad(dht), look_for, //
         dht.config.percentage_seek, "%",                  //
         cur, "%", all);

  if (cur < dht.config.percentage_seek) {
    // TODO if we can't mint new tx then next should be calculated base on when
    // soonest next tx timesout is so we can directly reuse it. (it should not
    // be the config.refresh_interval of 15min if we are under conf.p_seek)
    auto awake_next = awake_look_for_nodes(dht, out, look_for);
    result = std::min(result, awake_next);
    log::awake::contact_scan(dht);
  } else {
    // TODO 40% really?
    assertxs(false, cur, dht.config.percentage_seek);
  }

  return result;
}

static Node *
dht_activity(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  if (!dht::is_valid(sender)) {
    return nullptr;
  }

  DHT &dht = ctx.dht;

  /*request from self*/
  if (dht.id == sender.id) {
    return nullptr;
  }

  if (dht::is_blacklisted(dht, ctx.remote)) {
    return nullptr;
  }

  Node *result = find_contact(dht, sender);
  if (result) {
    timeout::update_receive(dht, result);

    if (!result->good) {
      result->good = true;
      assertx(dht.bad_nodes > 0);
      dht.bad_nodes--;
    }
  } else {

    Node contact(sender, ctx.remote, dht.now);
    result = dht::insert(dht, contact);
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
static dht::Node *
dht_request(dht::MessageContext &ctx, const dht::NodeId &sender, F f) noexcept {
  dht::Node *contact = dht_activity(ctx, sender);

  handle_ip_election(ctx, sender);
  if (contact) {
    contact->remote_activity = ctx.dht.now;
    f(*contact);
  } else {
    dht::Node n;
    n.id = sender;
    f(n);
  }

  return contact;
}

template <typename F>
static dht::Node *
dht_response(dht::MessageContext &ctx, const dht::NodeId &sender,
             F f) noexcept {
  dht::Node *contact = dht_activity(ctx, sender);

  handle_ip_election(ctx, sender);
  if (contact) {

    contact->remote_activity = ctx.dht.now;
    f(*contact);
  } else {
    /* phony */
    dht::Node n;
    n.id = sender;
    f(n);
  }

  return contact;
}

static void
print_raw(const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    printf("'%.*s': %zu", int(len), val, len);
  } else {
    printf("hex[");
    for (std::size_t i = 0; i < len; ++i) {
      printf("%hhX", (unsigned char)val[i]);
    }
    printf("](");
    for (std::size_t i = 0; i < len; ++i) {
      if (ascii::is_printable(val[i])) {
        printf("%c", val[i]);
      } else {
        printf("_");
      }
    }
    printf(")");
  }
}

static bool
bencode_any(sp::Buffer &p, const char *ctx) noexcept {
  /*any str*/ {
    const char *kit = nullptr;
    std::size_t klen = 0;

    const unsigned char *vit = nullptr;
    std::size_t vlen = 0;

    if (bencode::d::pair_ref(p, kit, klen, vit, vlen)) {
      printf("%s any[", ctx);
      print_raw(kit, klen);
      printf(", ");
      print_raw((const char *)vit, vlen);
      printf("] \n");
      return true;
    }
  }

  /*any int*/ {
    const char *kit = nullptr;
    std::size_t klen = 0;

    std::uint64_t value = 0;

    if (bencode::d::pair_ref(p, kit, klen, value)) {
      printf("%s any[", ctx);
      print_raw(kit, klen);
      printf(", %lu]\n", value);
      return true;
    }
  }

  /*any list*/ {
    // TODO
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

  dht_request(ctx, sender, [&ctx](auto &) {
    dht::DHT &dht = ctx.dht;
    krpc::response::ping(ctx.out, ctx.transaction, dht.id);
  });
  // XXX response

  return true;
} // ping::handle_request()

static bool
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  log::receive::res::ping(ctx);

  dht_response(ctx, sender, [](auto &node) { //
    node.ping_outstanding = 0;
  });

  return true;
} // ping::handle_response()

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  return krpc::d::response::ping(ctx, handle_response);
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return krpc::d::request::ping(ctx, handle_request);
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

  dht_request(ctx, sender, [&](auto &) {
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
      std::is_same<dht::Node, typename ContactType::value_type>::value, "");
  log::receive::res::find_node(ctx);

  dht_response(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) {
      dht::DHT &dht = ctx.dht;
      dht::Node node(contact, dht.now);
      auto res = dht::insert(dht, node); // TODO does this return existing?
      if (!res) {
        insert_unique(dht.bootstrap_contacts, node.contact);
      }
    });
  });

} // find_node::handle_response()

static void
handle_response_timeout(dht::DHT &dht, void *closure) noexcept {
  if (closure) {
    auto *bs = (Contact *)closure;
    delete bs;
  }

  assertx(dht.active_searches > 0);
  dht.active_searches--;
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
           void *closure) noexcept {
  log::transmit::error::find_node_response_timeout(dht, tx, sent);
  if (closure) {
    // arbitrary?
    if (nodes_good(dht) < 100) {
      auto *bs = (Contact *)closure;
      insert_unique(dht.bootstrap_contacts, *bs);
    }
  }

  handle_response_timeout(dht, closure);
} // find_node::on_timeout

static bool
on_response(dht::MessageContext &ctx, void *closure) noexcept {
  dht::DHT &dht = ctx.dht;

  Contact cap_copy;
  Contact *cap_ptr = nullptr;
  if (closure) {
    auto *bs = (Contact *)closure;

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
    dht::Token token; // TODO

    auto &nodes = dht.recycle_contact_list;
    sp::clear(nodes);

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
      sp::clear(nodes);
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
      return false;
    }

    if (!is_empty(nodes)) {
      if (cap_ptr) {
        // only remove bootstrap node if we have gotten some nodes from it
        remove(dht.bootstrap_contacts, *cap_ptr);
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

  dht_request(ctx, id, [&](auto &from) {
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
      dht::Node *closest[8] = {nullptr};
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
      std::is_same<dht::Node, typename ContactType::value_type>::value, "");

  log::receive::res::get_peers(ctx);
  dht::DHT &dht = ctx.dht;
  dht_request(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) {
      // search(NodeId) {
      /*test bloomfilter*/
      if (!test(search.searched, contact.id)) {
        /*insert into bloomfilter*/
        auto ires = insert(search.searched, contact.id);
        assertx(ires);
        insert_eager(search.queue, dht::K(contact, search.search.id));
      }
      // }

      /* sender returns the its closest contacts for search */
      dht::Node ins(contact, dht.now);
      auto res = dht::insert(dht, ins);
      if (!res) {
        insert_unique(dht.bootstrap_contacts, ins.contact);
      }
    });

    /* sender returns matching values for search query */
    for_each(result, [&search](const Contact &c) {
      /**/
      insert(search.result, c);
    });
  });
} // get_peers::handle_response

static void
dec(dht::SearchContext *ctx) noexcept {
  ctx->ref_cnt--;
  if (ctx->is_dead) {
    if (ctx->ref_cnt == 0) {
      delete ctx;
    }
  }
}

static void
on_timeout(dht::DHT &dht, const krpc::Transaction &tx, Timestamp sent,
           void *ctx) noexcept {
  log::transmit::error::get_peers_response_timeout(dht, tx, sent);
  auto search = (dht::SearchContext *)ctx;
  if (search) {
    dec(search);
  }
} // get_peers::on_timeout

static bool
on_response(dht::MessageContext &ctx, void *searchCtx) noexcept {
  // assertx(searchCtx);
  if (!searchCtx) {
    return true;
  }

  auto search_ctx = (dht::SearchContext *)searchCtx;
  dht::Search *search = find_search(ctx.dht, search_ctx);
  dec(search_ctx);

  // assertx(search);
  if (!search) {
    return true;
  }

  return bencode::d::dict(ctx.in, [&ctx, search](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_n = false;
    bool b_v = false;
    bool b_ip = false;
    bool b_p = false;

    dht::NodeId id;
    dht::Token token;

    dht::DHT &dht = ctx.dht;
    auto &nodes = dht.recycle_contact_list;
    clear(nodes);

    auto &values = dht.recycle_value_list;
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

    std::uint64_t p_out = 0; // TODO what is this?
    if (!b_p && bencode::d::pair(p, "p", p_out)) {
      b_p = true;
      goto Lstart;
    } else {
      assertx(pos == p.pos);
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      } else {
        assertx(pos == p.pos);
      }
    }

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
      if (bencode::d::peers(p, "values", values)) { // last
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
  dht_request(ctx, sender, [&](auto &from) {
    if (db::valid(dht, from, token)) {
      Contact peer(ctx.remote);
      if (implied_port) {
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

  dht_response(ctx, sender, [](auto &) { //

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
