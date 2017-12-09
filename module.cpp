#include "bencode.h"
#include "client.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include <algorithm>
#include <cassert>
#include <cstring>

//===========================================================
// Module
//===========================================================
namespace dht {

static bool
mintTransaction(DHT &dht, krpc::Transaction &t) noexcept {
  int r = rand();
  std::memcpy(t.id, &r, 2);
  t.length = 2;

  return true;
}

static Timeout
awake(DHT &, Client &, sp::Buffer &, time_t) noexcept;

/*Module*/
Module::Module() noexcept
    : query(nullptr)
    , response(nullptr)
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
inc_outstanding(Node *node) noexcept {
  std::uint8_t &data = node->ping_outstanding;
  if (data != ~std::uint8_t(0)) {
    ++data;
  }
}

} // namespace dht

namespace timeout {

static dht::Node *
take(dht::DHT &ctx, time_t now) noexcept {
  auto is_expired = [](auto &node, time_t cmp) { //
    time_t exp = dht::activity(node);
    return exp <= cmp;
  };

  dht::Node *result = nullptr;
  dht::Node *const head = ctx.timeout_node;
  dht::Node *current = head;
Lstart:
  if (current) {
    if (is_expired(*current, now)) {
      dht::Node *const next = current->timeout_next;
      timeout::unlink(ctx, current);

      if (!result) {
        result = current;
      } else {
        current->timeout_next = result;
        result = current;
      }

      if (next != head) {
        current = next;
        goto Lstart;
      } else {
        ctx.timeout_node = nullptr;
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
  contact->ping_sent = now;
  append_all(ctx, contact);
} // timeout::update()

} // namespace timeout

namespace dht {
// TODO
//-keep track of transaction ids we sent out in requests,bounded queue of
// active transaction with a timout only send out pings based on how many
// free slots in queue,delay the rest ready for ping the next 'awake' slot.
// TODO check that peer owns nodeid before change anything
// TODO calculate latency by having a list of active
// transaction{id,time_sent} latency = now - time_sent. To be used an
// deciding factor when inserting to a full not expandable bucket. Or
// deciding on which X best Contact Nodes when returning find_node/get_peers

/*pings*/
static Timeout
awake_ping(DHT &ctx, Client &client, sp::Buffer &out, time_t now) noexcept {
  {
    Node *timedout = timeout::take(ctx, now);
    for_each(timedout, [&ctx, &client, &out, now](Node *node) { //
      krpc::Transaction t;
      mintTransaction(ctx, t);

      krpc::request::ping(out, t, node->id);
      sp::flip(out);
      dht::send(client, node->peer, t, out);
      inc_outstanding(node);

      // Fake update activity otherwise if all nodes have to same timeout we
      // will spam out pings, ex: 3 noes timed out ,send ping, append, get the
      // next timeout date, since there is only 3 in the queue and we will
      // immediately awake and send ping  to the same 3 nodes
      node->ping_sent = now;
    });
    timeout::append_all(ctx, timedout);
  }

  /*calculate next timeout*/
  Config config;
  Node *const tHead = timeout::head(ctx);
  if (tHead) {

    time_t ping_secs_ago = now - tHead->ping_sent;
    time_t next_timeout = config.refresh_interval > ping_secs_ago
                              ? config.refresh_interval - ping_secs_ago
                              : 0;
    time_t normalized = std::max(config.min_timeout_interval, next_timeout);

    ctx.timeout_next = now + normalized;
    return Timeout(normalized * 1000);
  }

  // timeout queue is empty
  ctx.timeout_next = config.refresh_interval;
  return Timeout(-1);
}

static Timeout
awake_peer_db(DHT &ctx, time_t now) {
  return Timeout(0);
}

static Timeout
awake(DHT &ctx, Client &client, sp::Buffer &out, time_t now) noexcept {
  reset(out);
  // TODO timeout peer
  Timeout next(-1);
  if (ctx.timeout_next >= now) {
    Timeout node_timeout = awake_ping(ctx, client, out, now);
  }
  if (ctx.timeout_peer_next >= now) {
    Timeout peer_timeout = awake_peer_db(ctx, now);
  }
  // recalculate
  return now - ctx.timeout_next;
}

static Node *
dht_activity(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  if (!dht::is_valid(sender)) {
    return nullptr;
  }

  DHT &dht = ctx.dht;
  if (dht::is_blacklisted(dht, ctx.remote)) {
    return nullptr;
  }

  time_t now = ctx.now;

  Node *result = find_contact(dht, sender);
  if (result) {

    timeout::update(dht, result, now);
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

  dht::DHT &dht = ctx.dht;
  if (!dht::valid(dht, ctx.transaction)) {
    return nullptr;
  }

  dht::Node *contact = dht_activity(ctx, sender);
  if (contact) {
    contact->request_activity = ctx.now;
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
    contact->response_activity = ctx.now;
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
  dht_response(ctx, sender, [&ctx](auto &) { //
    dht::DHT &dht = ctx.dht;
    krpc::response::ping(ctx.out, ctx.transaction, dht.id);
  });
  // XXX response

  return true;
} // ping::handle_request()

static bool
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
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
  dht_request(ctx, sender, [&](auto &) {
    dht::DHT &dht = ctx.dht;
    constexpr std::size_t capacity = 8;

    const krpc::Transaction &t = ctx.transaction;
    dht::Node *result[dht::Bucket::K] = {nullptr};
    dht::find_closest(dht, search, result);
    const dht::Node **r = (const dht::Node **)&result;

    krpc::response::find_node(ctx.out, t, dht.id, r, capacity);
  });
} // find_node::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const sp::list<dht::Node> &contacts) noexcept {
  dht_response(ctx, sender, [&](auto &) {
    dht::DHT &dht = ctx.dht;
    for_each(contacts, [&](const auto &contact) { //

      dht::Node ins(contact, ctx.now);
      dht::insert(dht, ins);
    });

  });

} // find_node::handle_response()

static bool
on_response(dht::MessageContext &ctx) {
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
on_request(dht::MessageContext &ctx) {
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
}

} // namespace find_node

//===========================================================
// get_peers
//===========================================================
namespace get_peers {
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &id,
               const dht::Infohash &search) noexcept {
  dht_request(ctx, id, [&](auto &) {
    dht::Token token;
    dht::DHT &dht = ctx.dht;
    dht::mintToken(dht, ctx.remote.ip, token, ctx.now);

    const krpc::Transaction &t = ctx.transaction;
    const dht::KeyValue *result = lookup::lookup(dht, search, ctx.now);
    if (result) {
      // TODO??
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
                const dht::Token &token,
                const sp::list<dht::Contact> &values) noexcept {
  /*
   * infohash lookup query found result, sender returns requested data.
   */
  dht_response(ctx, sender, [](auto &) {
    // XXX
  });
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &,
                const sp::list<dht::Node> &contacts) noexcept {
  /*
   * sender has no information for queried infohash, returns the closest
   * contacts.
   */
  dht::DHT &dht = ctx.dht;
  dht_request(ctx, sender, [&](auto &) {
    for_each(contacts, [&](const auto &contact) { //

      dht::Node ins(contact, ctx.now);
      dht::insert(dht, ins);
    });
  });
}

static bool
on_response(dht::MessageContext &ctx) {
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
on_request(dht::MessageContext &ctx) {
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

      lookup::insert(dht, infohash, peer, ctx.now);
    });
  }
  krpc::response::announce_peer(ctx.out, ctx.transaction, dht.id);
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  dht_response(ctx, sender, [](auto &) { //

  });
}

static bool
on_response(dht::MessageContext &ctx) {
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
on_request(dht::MessageContext &ctx) {
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
on_response(dht::MessageContext &ctx) {
  printf("unknow response query type %s\n", ctx.query);
  return true;
}

static bool
on_request(dht::MessageContext &ctx) {
  printf("unknow request query type %s\n", ctx.query);

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
