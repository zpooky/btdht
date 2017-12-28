#include "Log.h"
#include "bencode.h"
#include "client.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include "transaction.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

//===========================================================
// Module
//===========================================================
namespace dht {

static Timeout
awake(DHT &, sp::Buffer &) noexcept;

/*Module*/
Module::Module() noexcept
    : query(nullptr)
    , response(nullptr)
    , response_timeout(nullptr)
    , request(nullptr) {
}

/*Modules*/
Modules::Modules() noexcept
    : module{}
    , length(0)
    , on_awake(awake) {
}

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

static void
Return(dht::DHT &ctx, dht::Node *ret) noexcept {
  assert(ret->timeout_next == nullptr);
  assert(ret->timeout_priv == nullptr);

  dht::Node *const head = ctx.timeout_node;
  if (head) {
    dht::Node *next = head->timeout_next;
    head->timeout_next = ret;
    next->timeout_priv = ret;

    ret->timeout_priv = head;
    ret->timeout_next = next;
  } else {
    ctx.timeout_node = ret->timeout_priv = ret->timeout_next = ret;
  }
}

static dht::Node *
head(dht::DHT &ctx) noexcept {
  return ctx.timeout_node;
} // timeout::head()

static void
update(dht::DHT &ctx, dht::Node *const contact, time_t now) noexcept {
  assert(contact);
  assert(now >= dht::activity(*contact));

  unlink(ctx, contact);
  contact->ping_sent = now; // TODO change to activity
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

  /*calculate next timeout*/
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
  std::size_t searches = dht.bootstrap_ongoing_searches * dht::Bucket::K;
  missing_contacts -= std::min(missing_contacts, searches);

  auto inc_ongoing = [&dht, &missing_contacts]() {
    std::size_t K = dht::Bucket::K;
    missing_contacts -= std::min(missing_contacts, K);
    dht.bootstrap_ongoing_searches++;
  };

  // Lstart:
  NodeId id;
  dht::randomize(id);

  auto search_id = [&dht, &id](dht::Bucket &b) -> NodeId & {
    Lretry:
      if (b.bootstrap_generation == 0) {
        b.bootstrap_generation++;
        return dht.id;
      }

      Config config;
      if (b.bootstrap_generation >= config.bootstrap_generation_max) {
        b.bootstrap_generation = 0;
        goto Lretry;
      }
      b.bootstrap_generation++;
      return id;
  };
  // TODO self should not be in boottstrap list
  // TODO how to handle that bootstrap contact is in current Bucket
  // XXX how to handle the same bucket will be reselected to send find_nodes
  // multiple times in a row

  // TODO How to avoid flooding the same nodes with request especially when we
  // only have a frew nodes in routing table?

  // XXX if no good node is avaiable try bad/questionable nodes
  auto &bs = dht.bootstrap_contacts;
  for_each(bs, [&dht, &out, inc_ongoing, id](const Contact &remote) {

    bool res = client::find_node(dht, out, remote, id);
    if (res) {
      inc_ongoing();
    }
    return res;
  });

  if (missing_contacts > 0) {
    dht::Bucket *const b = dht::bucket_for(dht, id);
    if (b) {
      const NodeId &sid = search_id(*b);
      bool ok = for_all(*b, [&dht, &out, inc_ongoing, sid](Node &remote) {
        if (dht::is_good(dht, remote)) {

          Contact &c = remote.contact;
          bool res = client::find_node(dht, out, c, sid);
          if (res) {
            inc_ongoing();
          }
          return res;
        }
        return true;
      });

      if (ok) {
        // goto Lstart;
      }
    }
  }
}

static Timeout
awake(DHT &dht, sp::Buffer &out) noexcept {
  Config config;
  Timeout next(config.refresh_interval);

  if (dht.timeout_next >= dht.now) {
    next = std::min(awake_ping(dht, out), next);
  }
  if (dht.timeout_peer_next >= dht.now) {
    next = std::min(awake_peer_db(dht), next);
  }

  {
    auto percentage = [](std::uint32_t t, std::uint32_t c) {
      return c / (t / 100);
    };
    std::uint32_t good = dht.total_nodes - dht.bad_nodes;
    std::uint32_t total = std::max(std::uint32_t(365), good); // TODO
    std::uint32_t look_for = total - good;
    if (percentage(total, good) < config.percentage_seek) {
      look_for_nodes(dht, out, look_for);
    }
  }

  // Recalculate
  return next;
}

static Node *
dht_activity(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  if (!dht::is_valid(sender)) {
    return nullptr;
  }

  DHT &dht = ctx.dht;
  /*request from self*/
  if (std::memcmp(dht.id.id, sender.id, sizeof(sender.id)) == 0) {
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

template <typename F>
static dht::Node *
dht_request(dht::MessageContext &ctx, const dht::NodeId &sender, F f) noexcept {
  dht::Node *contact = dht_activity(ctx, sender);
  if (contact) {
    contact->request_activity = ctx.dht.now;
    f(*contact);
  }

  return contact;
}

template <typename F>
static dht::Node *
dht_response(dht::MessageContext &ctx, const dht::NodeId &sender,
             F f) noexcept {
  dht::Node *contact = dht_activity(ctx, sender);
  if (contact) {
    contact->response_activity = ctx.dht.now;
    f(*contact);
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

  dht_response(ctx, sender, [&ctx](auto &) { //
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
on_response(dht::MessageContext &ctx) noexcept {
  return krpc::d::response::ping(ctx, handle_response);
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return krpc::d::request::ping(ctx, handle_request);
}

void
setup(dht::Module &module) noexcept {
  module.query = "ping";
  module.request = on_request;
  module.response = on_response;
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
    constexpr std::size_t capacity = dht::Bucket::K;

    const krpc::Transaction &t = ctx.transaction;
    dht::Node *result[capacity] = {nullptr};
    dht::find_closest(dht, search, result);
    const dht::Node **r = (const dht::Node **)&result;

    krpc::response::find_node(ctx.out, t, dht.id, r, capacity);
  });
} // find_node::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const sp::list<dht::Node> &contacts) noexcept {
  log::receive::res::find_node(ctx);

  dht::DHT &dht = ctx.dht;
  assert(dht.bootstrap_ongoing_searches > 0);
  dht.bootstrap_ongoing_searches--;

  dht_response(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) { //

      dht::Node node(contact, dht.now);
      dht::insert(dht, node);
    });

  });

} // find_node::handle_response()

static void
handle_response_timeout(dht::DHT &dht) noexcept {
  assert(dht.bootstrap_ongoing_searches > 0);
  dht.bootstrap_ongoing_searches--;
}

static bool
on_response(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    bool b_id = false;
    bool b_n = false;

    dht::NodeId id;

    dht::DHT &dht = ctx.dht;
    sp::list<dht::Node> &nodes = dht.contact_list;
    sp::clear(nodes);

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }
    if (!bencode::d::pair(p, "nodes", nodes)) {
      b_n = true;
      goto Lstart;
    }
    if (!(b_id && b_n)) {
      return false;
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

  dht_request(ctx, id, [&](auto &) {
    dht::Token token;
    dht::DHT &dht = ctx.dht;
    lookup::mint_token(dht, ctx.remote.ip, token);

    const krpc::Transaction &t = ctx.transaction;
    const dht::KeyValue *result = lookup::lookup(dht, search);
    if (result) {

      krpc::response::get_peers(ctx.out, t, dht.id, token, result->peers);
    } else {
      constexpr std::size_t capacity = 8;
      dht::Node *closest[dht::Bucket::K] = {nullptr};
      dht::find_closest(dht, search, closest);
      const dht::Node **r = (const dht::Node **)&closest;

      krpc::response::get_peers(ctx.out, t, dht.id, token, r, capacity);
    }
  });
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &, // XXX store token to used for announce
                const sp::list<dht::Contact> &) noexcept {
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

      dht::Node ins(contact, dht.now);
      dht::insert(dht, ins);
    });
  });
}

static bool
on_response(dht::MessageContext &ctx) noexcept {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_n = false;
    bool b_v = false;

    dht::NodeId id;
    dht::Token token;

    dht::DHT &dht = ctx.dht;
    sp::list<dht::Node> &nodes = dht.contact_list;
    sp::clear(nodes);

    sp::list<dht::Contact> &values = dht.value_list;
    sp::clear(values);

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (!b_t && bencode::d::pair(p, "token", token.id)) {
      b_t = true;
      goto Lstart;
    }

    /*closes K nodes*/
    if (!b_n && bencode::d::pair(p, "nodes", nodes)) {
      b_n = true;
      goto Lstart;
    }

    if (!b_v && bencode::d::pair(p, "values", values)) {
      b_v = true;
      goto Lstart;
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
  if (lookup::valid(dht, token)) {
    dht_request(ctx, sender, [&](auto &) {
      dht::Contact peer;
      peer.ip = ctx.remote.ip;
      if (implied_port) {
        peer.port = ctx.remote.port;
      } else {
        peer.port = port;
      }

      lookup::insert(dht, infohash, peer);
    });
  }
  krpc::response::announce_peer(ctx.out, ctx.transaction, dht.id);
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  log::receive::res::announce_peer(ctx);

  dht_response(ctx, sender, [](auto &) { //

  });
}

static bool
on_response(dht::MessageContext &ctx) noexcept {
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
    if (!b_t && bencode::d::pair(p, "token", token.id)) {
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
on_response(dht::MessageContext &ctx) noexcept {
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
