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
#include <cassert>
#include <collection/Array.h>
#include <cstring>
#include <prng/util.h>
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
  // 2.find_node
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
    assert(node->timeout_next == nullptr);
    assert(node->timeout_priv == nullptr);
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
update_receive(dht::DHT &, dht::Node *node) noexcept {
  // TODO rename response_activity to receive_activity?
  // node->response_activity = dht.now;
  // return update(ctx, contact, now);
} // timeout::update()


} // namespace timeout

namespace dht {

/*pings*/
static Timeout
on_awake_ping(DHT &ctx, sp::Buffer &out) noexcept {
  Config config;
  timeout::for_all(ctx, config.refresh_interval, [&out](auto &dht, auto &node) {
    if (client::ping(dht, out, node)) {
      inc_outstanding(node);
      // timeout::update_send(dht, node);
      node.req_sent = dht.now;

      /*
       * Fake update activity otherwise if all nodes have to
       * same timeout we will spam out pings, ex: 3 noes timed
       * out, send ping, append, get the next timeout date,
       * since there is only 3 in the queue and we will
       * immediately awake and send ping  to the same 3 nodes
       */
      node.req_sent = dht.now;
      assert(node.timeout_next == nullptr);
      return true;
    }
    return false;
  });

  /*Calculate next timeout*/
  Node *const tHead = timeout::head(ctx);
  if (tHead) {
    const Timestamp next = tHead->req_sent + config.refresh_interval;
    ctx.timeout_next = next;

    if (next > ctx.now) {
      Timestamp next_seconds = next - ctx.now;
      // return std::max(config.min_timeout_interval, next_seconds);
      return config.min_timeout_interval > next_seconds
                 ? Timeout(config.min_timeout_interval)
                 : next_seconds;
    }

    return Timeout(config.min_timeout_interval);
  } else {
    // timeout queue is empty
    ctx.timeout_next = ctx.now + config.refresh_interval;
  }

  return config.refresh_interval;
}

static Timeout
on_awake_peer_db(DHT &, sp::Buffer &) noexcept {
  // {
  // Lstart:
  //   Peer *const peer = timeout::take(dht.now, dht.timeout_peer, 1);
  //   if (peer) {
  //     assert(peer->timeout_next == nullptr);
  //     assert(peer->timeout_priv == nullptr);
  //   }
  // }
  // TODO

  Config config;
  return config.refresh_interval;
}

static Timeout
awke_look_for_nodes(DHT &dht, sp::Buffer &out, std::size_t missing_contacts) {
  // XXX change to node circular buffer with timestamp of last sent
  std::size_t searches = dht.active_searches * dht::Bucket::K;
  missing_contacts -= std::min(missing_contacts, searches);

  auto inc_ongoing = [&dht, &missing_contacts]() {
    std::size_t K = dht::Bucket::K;
    missing_contacts -= std::min(missing_contacts, K);
    dht.active_searches++;
  };

  Config cfg;
  bool bs_sent = false;
Lstart:
  // TODO verify bad_nodes works...
  // TODO bootstrap should be last. Tag bucket with bootstrap generation only
  // use bootstrap if we get the same bucket and we haven't sent any to
  // bootstrap this generation.

  // XXX self should not be in bootstrap list
  // XXX how to handle the same bucket will be reselected to send find_nodes
  // multiple times in a row

  // TODO How to avoid flooding the same nodes with request especially when we
  // only have a frew nodes in routing table?

  // XXX if no good node is avaiable try bad/questionable nodes

  if (missing_contacts > 0) {
    bool ok_sent = true;

    std::size_t sent_count = 0;
    const auto &sid = dht.id;
    timeout::for_all(dht, cfg.refresh_interval, [&](auto &ctx, Node &remote) {
      if (dht::is_good(ctx, remote)) {

        const Contact &c = remote.contact;
        bool res = client::find_node(ctx, out, c, sid, nullptr);
        if (res) {
          ++sent_count;
          inc_ongoing();
          remote.req_sent = dht.now;
        }

        return res;
      }

      return true;
    });

    if (!bs_sent) {
      // printf("#bootstrap nodes\n");
      auto &bs = dht.bootstrap_contacts;
      // TODO prune non good bootstrap nodes
      // XXX shuffle bootstrap list just before sending
      for_all(bs, [&dht, &out, inc_ongoing, sid](const Contact &remote) {

        auto *closure = new (std::nothrow) Contact(remote);
        bool res = client::find_node(dht, out, remote, sid, closure);
        if (res) {
          inc_ongoing();
        }

        return res;
      });
      bs_sent = true;
    }

    if (ok_sent) {
      if (sent_count > 0) {
        goto Lstart;
      }
    }
  } // missing_contacts > 0

  return cfg.refresh_interval;
} // awke_look_for_nodes()

static Timeout
on_awake(DHT &dht, sp::Buffer &out) noexcept {
  Config config;
  Timeout next(config.refresh_interval);

  {
    auto percentage = [](std::uint32_t t, std::uint32_t c) -> std::size_t {
      return std::size_t(c) / std::size_t(t / std::size_t(100));
    };

    const std::uint32_t good = dht.total_nodes - dht.bad_nodes;
    // TODO
    auto total = std::max(std::uint32_t(dht::max_routing_nodes(dht)), good);
    const std::uint32_t look_for = total - good;
    printf("good[%u], total[%u], bad_nodes[%u], look_for[%u], "
           "config.percentage_seek[%zu], "
           "current percentage[%zu], max[%u]\n",
           good, dht.total_nodes, dht.bad_nodes, look_for,
           config.percentage_seek, percentage(total, good),
           dht::max_routing_nodes(dht));
    if (percentage(total, good) < config.percentage_seek) {
      next = std::min(next, awke_look_for_nodes(dht, out, look_for));
      log::awake::contact_scan(dht);
    }
  }

  return next;
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
      assert(dht.bad_nodes > 0);
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
    //   // assert(false);
    // }
  }
}

template <typename F>
static dht::Node *
dht_request(dht::MessageContext &ctx, const dht::NodeId &sender, F f) noexcept {
  dht::Node *contact = dht_activity(ctx, sender);

  handle_ip_election(ctx, sender);
  if (contact) {
    contact->request_activity = ctx.dht.now;
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

    contact->response_activity = ctx.dht.now;
    f(*contact);
  } else {
    /* phony */
    dht::Node n;
    n.id = sender;
    f(n);
  }

  return contact;
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
  // printf("response ping\n");
  return krpc::d::response::ping(ctx, handle_response);
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  // printf("request ping\n");
  return krpc::d::request::ping(ctx, handle_request);
}

static void
on_timeout(dht::DHT &dht, void *arg) noexcept {
  log::transmit::error::ping_response_timeout(dht);
  assert(arg == nullptr);
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

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const sp::list<dht::Node> &contacts) noexcept {
  log::receive::res::find_node(ctx);

  dht_response(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) {

      dht::DHT &dht = ctx.dht;
      dht::Node node(contact, dht.now);
      auto res = dht::insert(dht, node); // TODO does this return existing?
      if (!res) {
        insert(dht.bootstrap_contacts, node.contact); // TODO insert unique
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
  assert(dht.active_searches > 0);
  dht.active_searches--;
}

static void
on_timeout(dht::DHT &dht, void *closure) noexcept {
  log::transmit::error::find_node_response_timeout(dht);
  handle_response_timeout(dht, closure);
}

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

    sp::list<dht::Node> &nodes = dht.recycle_contact_list;
    sp::clear(nodes);

    std::uint64_t p_param = 0;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }

    // optional
    if (!b_n) {
      sp::clear(nodes);
      if (bencode::d::nodes(p, "nodes", nodes)) {
        b_n = true;
        goto Lstart;
      }
    }

    if (!b_t && bencode::d::pair(p, "token", token)) {
      b_t = true;
      goto Lstart;
    }

    // optional
    if (!b_p && bencode::d::pair(p, "p", p_param)) {
      b_p = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assert(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (!(b_id)) {
      return false;
    }

    if (!is_empty(nodes)) {
      if (cap_ptr) {
        // only remove bootstrap node if we have gotten some nodes from it
        sp::remove(dht.bootstrap_contacts, *cap_ptr);
      }
    }

    handle_response(ctx, id, nodes);
    return true;
  });
}

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

    if (!(b_id && b_t)) {
      return false;
    }

    handle_request(ctx, id, target);
    return true;
  });
}

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
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &, const sp::list<Contact> &result,
                dht::Search &search) noexcept {
  // XXX store token to used for announce
  log::receive::res::get_peers(ctx);
  /*
   * infohash lookup query found result, sender returns requested data.
   */
  dht_response(ctx, sender, [&search, &result](auto &) {
    // search(NodeId) {
    for_each(result, [&search](const Contact &c) {
      /**/
      insert(search.result, c);
    });
    // }
  });
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &, // XXX store token to used for announce
                const sp::list<dht::Node> &contacts,
                dht::Search &search) noexcept {
  log::receive::res::get_peers(ctx);
  /*
   * sender has no information for queried infohash, returns the closest
   * contacts.
   */
  dht::DHT &dht = ctx.dht;
  dht_request(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) {
      // search(NodeId) {
      /*test bloomfilter*/
      if (!test(search.searched, contact.id)) {
        /*insert into bloomfilter*/
        assert(insert(search.searched, contact.id));
        insert_eager(search.queue, dht::K(contact, search.search.id));
      }
      // }

      dht::Node ins(contact, dht.now);
      auto res = dht::insert(dht, ins);
      if (!res) {
        insert(dht.bootstrap_contacts, ins.contact);
      }
    });
  });
}

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
on_timeout(dht::DHT &dht, void *ctx) noexcept {
  log::transmit::error::get_peers_response_timeout(dht);
  auto search = (dht::SearchContext *)ctx;
  if (search) {
    dec(search);
  }
}

static bool
on_response(dht::MessageContext &ctx, void *searchCtx) noexcept {
  assert(searchCtx);
  if (!searchCtx) {
    return true;
  }

  auto search_ctx = (dht::SearchContext *)searchCtx;
  dht::Search *search = find_search(ctx.dht, search_ctx);
  dec(search_ctx);

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
    sp::list<dht::Node> &nodes = dht.recycle_contact_list;
    sp::clear(nodes);

    sp::list<Contact> &values = dht.recycle_value_list;
    sp::clear(values);

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (!b_t && bencode::d::pair(p, "token", token)) {
      b_t = true;
      goto Lstart;
    }

    std::uint64_t p_out = 0; // TODO what is this?
    if (!b_p && bencode::d::pair(p, "p", p_out)) {
      b_p = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assert(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    /*closes K nodes*/
    if (!b_n) {
      sp::clear(nodes);
      if (bencode::d::nodes(p, "nodes", nodes)) {
        b_n = true;
        goto Lstart;
      }
    }

    if (!b_v) {
      sp::clear(values);
      if (bencode::d::peers(p, "values", values)) {
        b_v = true;
        goto Lstart;
      }
    }

    if (b_id && b_t && b_n) {
      handle_response(ctx, id, token, nodes, *search);
      return true;
    }

    if (b_id && b_t && b_v) {
      handle_response(ctx, id, token, values, *search);
      return true;
    }

    return false;
  });
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    bool b_id = false;
    bool b_ih = false;

    dht::NodeId id;
    dht::Infohash infohash;

  start:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto start;
    }

    if (!b_ih && bencode::d::pair(p, "info_hash", infohash.id)) {
      b_ih = true;
      goto start;
    }

    if (!(b_id && b_ih)) {
      return false;
    }

    handle_request(ctx, id, infohash);
    return true;
  });
}

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
    if (!bencode::d::pair(p, "id", id.id)) {
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
