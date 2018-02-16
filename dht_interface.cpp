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
awake(DHT &dht, sp::Buffer &out) noexcept;
}

namespace interface_dht {

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  ping::setup(modules.module[i++]);
  find_node::setup(modules.module[i++]);
  get_peers::setup(modules.module[i++]);
  announce_peer::setup(modules.module[i++]);
  error::setup(modules.module[i++]);
  //
  modules.on_awake = &dht::awake;

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
take(time_t now, T *&the_head, std::size_t max) noexcept {
  auto is_expired = [](auto &node, time_t cmp) { //
    time_t exp = dht::activity(node);
    return exp <= cmp;
  };

  T *result = nullptr;
  T *const head = the_head;
  T *current = head;
  std::size_t cnt = 0;
Lstart:
  if (current && cnt < max) {
    if (is_expired(*current, now)) {
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

static dht::Node *
head(dht::DHT &ctx) noexcept {
  return ctx.timeout_node;
} // timeout::head()

static void
update(dht::DHT &ctx, dht::Node *const contact, time_t now) noexcept {
  assert(contact);
  assert(now >= dht::activity(*contact));

  unlink(ctx, contact);
  assert(!contact->timeout_next);
  assert(!contact->timeout_priv);
  contact->ping_sent = now; // TODO change ping_sent to activity
  append_all(ctx, contact);
} // timeout::update()

} // namespace timeout

namespace dht {

/*pings*/
static Timeout
awake_ping(DHT &ctx, sp::Buffer &out) noexcept {
  {
  Lstart:
    Node *const node = timeout::take(ctx.now, ctx.timeout_node, 1);
    if (node) {
      assert(node->timeout_next == nullptr);
      assert(node->timeout_priv == nullptr);
      if (node->good) {
        if (dht::should_mark_bad(ctx, *node)) {
          node->good = false;
          ctx.bad_nodes++;
        }
      }

      if (client::ping(ctx, out, *node)) {
        inc_outstanding(*node);

        // Fake update activity otherwise if all nodes have to same timeout we
        // will spam out pings, ex: 3 noes timed out, send ping, append, get the
        // next timeout date, since there is only 3 in the queue and we will
        // immediately awake and send ping  to the same 3 nodes
        node->ping_sent = ctx.now;
        assert(node->timeout_next == nullptr);
        timeout::append_all(ctx, node);

        goto Lstart;
      } else {
        timeout::Return(ctx, node);
      }
    }
  }

  /*Calculate next timeout*/
  Config config;
  Node *const tHead = timeout::head(ctx);
  if (tHead) {
    const time_t next = tHead->ping_sent + config.refresh_interval;
    ctx.timeout_next = next;
    if (next > ctx.now) {

      time_t next_seconds = next - ctx.now;
      time_t normalized = std::max(config.min_timeout_interval, next_seconds);

      // seconds to ms
      return Timeout(normalized * 1000);
    }
  } else {
    // timeout queue is empty
    ctx.timeout_next = ctx.now + config.refresh_interval;
  }

  return Timeout(config.refresh_interval);
}

static Timeout
awake_peer_db(DHT &) noexcept {
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
  return Timeout(config.refresh_interval);
}

static void
look_for_nodes(DHT &dht, sp::Buffer &out, std::size_t missing_contacts) {
  std::size_t searches = dht.active_searches * dht::Bucket::K;
  missing_contacts -= std::min(missing_contacts, searches);

  auto inc_ongoing = [&dht, &missing_contacts]() {
    std::size_t K = dht::Bucket::K;
    missing_contacts -= std::min(missing_contacts, K);
    dht.active_searches++;
  };

  Config config;
  bool bs_sent = false;
  std::size_t bucket_not_used = 0;
Lstart:
  NodeId id;
  randomize(dht.random, id);

  auto search_id = [&dht, &id, config](dht::Bucket &b) -> NodeId & {
    Lretry:
      if (b.bootstrap_generation == 0) {
        b.bootstrap_generation++;
        return dht.id;
      }

      if (b.bootstrap_generation >= config.bootstrap_generation_max) {
        b.bootstrap_generation = 0;
        goto Lretry;
      }

      b.bootstrap_generation++;
      return id;
  };
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
  // TODO look_for_nodes should be called always each 15min
  auto copy = [](Bucket &in, sp::StaticArray<Node *, Bucket::K> &outp) {
    for_each(in, [&outp](Node &remote) { //
      insert(outp, &remote);
    });
  };

  if (missing_contacts > 0) {
    bool ok_sent = true;
    dht::Bucket *const b = dht::bucket_for(dht, id);
    if (b) {
      sp::StaticArray<Node *, Bucket::K> l;
      copy(*b, l);
      shuffle(dht.random, l);
      // printf("#routing table nodes: %zu\n", l.length);

      std::size_t sent_count = 0;
      const std::time_t next(b->find_node + config.bucket_find_node_spam);
      if (next <= dht.now) {
        const NodeId &sid = search_id(*b);
        ok_sent = for_all(
            l, [&dht, &out, inc_ongoing, sid, &sent_count](Node *remote) {
              if (dht::is_good(dht, *remote)) {

                Contact &c = remote->contact;
                bool res = client::find_node(dht, out, c, sid, nullptr);
                if (res) {
                  ++sent_count;
                  inc_ongoing();
                }

                return res;
              }

              return true;
            });
      }

      if (sent_count == 0) {
        ++bucket_not_used;
      }
    } else {
      ++bucket_not_used;
    }

    if (!bs_sent) {
      printf("#bootstrap nodes\n");
      auto &bs = dht.bootstrap_contacts;
      // TODO prune non good bootstrap nodes
      // XXX shuffle bootstrap list just before sending
      for_all(bs, [&dht, &out, inc_ongoing, id](const Contact &remote) {

        auto *closure = new (std::nothrow) Contact(remote);
        bool res = client::find_node(dht, out, remote, id, closure);
        if (res) {
          inc_ongoing();
        }

        return res;
      });
      bs_sent = true;
    }

    if (ok_sent) {
      if (bucket_not_used < config.max_bucket_not_find_node) {
        goto Lstart;
      }
    }
  } // missing_cntacts > 0
} // look_for_nodes()

static Timeout
awake(DHT &dht, sp::Buffer &out) noexcept {
  Config config;
  Timeout next(config.refresh_interval);

  if (dht.now >= dht.timeout_next) {
    Timeout ap = awake_ping(dht, out);
    next = std::min(ap, next);
  }
  log::awake::contact_ping(dht, 0);

  if (dht.now >= dht.timeout_peer_next) {
    Timeout ap = awake_peer_db(dht);
    next = std::min(ap, next);
  }
  log::awake::peer_db(dht, 0);

  {
    auto percentage = [](std::uint32_t t, std::uint32_t c) -> std::size_t {
      return std::size_t(c) / std::size_t(t / std::size_t(100));
    };

    const std::uint32_t good = dht.total_nodes - dht.bad_nodes;
    const std::uint32_t total =
        std::max(std::uint32_t(dht::max_routing_nodes(dht)), good); // TODO
    const std::uint32_t look_for = total - good;
    printf("good[%u], total[%u], bad_nodes[%u], look_for[%u], "
           "config.percentage_seek[%zu], "
           "current percentage[%zu], max[%u]\n",
           good, dht.total_nodes, dht.bad_nodes, look_for,
           config.percentage_seek, percentage(total, good),
           dht::max_routing_nodes(dht));
    if (percentage(total, good) < config.percentage_seek) {
      look_for_nodes(dht, out, look_for);
      log::awake::contact_scan(dht);
    }
  }

  // Recalculate
  return next * 1000;
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

  time_t now = dht.now;

  Node *result = find_contact(dht, sender);
  if (result) {
    timeout::update(dht, result, now);

    if (!result->good) {
      result->good = true;
      assert(dht.bad_nodes > 0);
      dht.bad_nodes--;
    }
  } else {

    Node contact(sender, ctx.remote, now);
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
timeout(dht::DHT &dht, void *arg) noexcept {
  log::transmit::error::ping_response_timeout(dht);
  assert(!arg);
}

void
setup(dht::Module &module) noexcept {
  module.query = "ping";
  module.request = on_request;
  module.response = on_response;
  module.response_timeout = timeout;
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
    for_each(contacts, [&](const auto &contact) { //

      // TODO handle self insert
      dht::DHT &dht = ctx.dht;
      dht::Node node(contact, dht.now);
      dht::insert(dht, node);

    });
  });

} // find_node::handle_response()

static void
handle_response_timeout(dht::DHT &dht, void *closure) noexcept {
  if (closure) {
    auto *bs = (Contact *)closure;
    delete bs;
  }
  log::transmit::error::find_node_response_timeout(dht);

  assert(dht.active_searches > 0);
  dht.active_searches--;
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
        remove_first(dht.bootstrap_contacts, [&cap_ptr](const auto &cmp) {
          /**/
          return cmp == *cap_ptr;
        });
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
  module.response_timeout = handle_response_timeout;
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
                const dht::Token &, // XXX store token to used for announce
                const sp::list<Contact> &) noexcept {
  log::receive::res::get_peers(ctx);
  /*
   * infohash lookup query found result, sender returns requested data.
   */
  dht_response(ctx, sender, [](auto &) {
    // XXX
  });
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &, // XXX store token to used for announce
                const sp::list<dht::Node> &contacts) noexcept {
  log::receive::res::get_peers(ctx);
  /*
   * sender has no information for queried infohash, returns the closest
   * contacts.
   */
  dht::DHT &dht = ctx.dht;
  dht_request(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) { //
      // TODO handle self insert

      dht::Node ins(contact, dht.now);
      dht::insert(dht, ins);
    });
  });
}

static bool
on_response(dht::MessageContext &ctx, void *) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_n = false;
    bool b_v = false;
    bool b_ip = false;

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
      handle_response(ctx, id, token, nodes);
      return true;
    }

    if (b_id && b_t && b_v) {
      handle_response(ctx, id, token, values);
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
