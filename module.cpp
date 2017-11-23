#include "bencode.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include "udp.h"
#include <algorithm>
#include <cassert>
#include <cstring>

//===========================================================
// Module
//===========================================================
namespace dht {

static bool
mintTransaction(DHT &dht, time_t now, krpc::Transaction &t) noexcept {
  int r = rand();
  std::memcpy(t.id, &r, 2);

  uint16_t seq = ++dht.sequence;
  std::memcpy(t.id + 2, &seq, 2);

  t.length = 4;
  // TODO keep track of transaction

  return true;
}

static Timeout
awake(DHT &, fd &, sp::Buffer &, time_t) noexcept;

/*MessageContext*/
MessageContext::MessageContext(const char *q, DHT &p_dht,
                               bencode::d::Decoder &p_in, sp::Buffer &p_out,
                               const krpc::Transaction &p_t, Contact p_remote,
                               time_t p_now) noexcept
    : query(q)
    , dht{p_dht}
    , in{p_in}
    , out{p_out}
    , transaction{p_t}
    , remote{p_remote}
    , now{p_now} {
}

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
increment_outstanding(Node *node) noexcept {
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
  dht::Node *head = ctx.timeout_head;
Lstart:
  if (head) {
    if (is_expired(*head, now)) {
      // iterate
      head = head->timeout_next;

      // build result
      head->timeout_next = result;
      result = head;

      // repeat
      goto Lstart;
    }
  }

  // updated head
  ctx.timeout_head = head;

  // clear tail if head is empty
  if (head == nullptr)
    ctx.timeout_tail = nullptr;

  return result;
} // timeout::take()

static dht::Node *
head(dht::DHT &ctx) noexcept {
  return ctx.timeout_head;
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
// TODO lookup table values lifetime
// TODO handle out of order request kv
// TODO ignore unknown request kv

static Timeout
awake(DHT &ctx, fd &udp, sp::Buffer &out, time_t now) noexcept {
  reset(out);
  if (ctx.timeout_next >= now) {
    Node *timedout = timeout::take(ctx, now);
    for_each(timedout, [&ctx, &udp, &out, now](Node *node) { //
      krpc::Transaction t;
      mintTransaction(ctx, now, t);

      krpc::request::ping(out, t, node->id);
      sp::flip(out);
      udp::send(udp, node->peer, out);
      increment_outstanding(node);

      // fake update activity otherwise if all nodes have to same timeout we
      // will spam out pings, ex: 3 noes timed out ,send ping, append, get the
      // next timeout date, since there is only 3 in the queue and we will
      // immediately awake and send ping  to the same 3 nodes
      node->ping_sent = now;
    });
    timeout::append_all(ctx, timedout);

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

  Node *contact = find_contact(dht, sender);
  if (contact) {
    timeout::update(dht, contact, now);
  } else {
    Node c(sender, ctx.remote, now);
    contact = dht::insert(dht, c);
  }

  return contact;
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
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  dht_response(ctx, sender, [](auto &) { //
  });

  dht::DHT &dht = ctx.dht;
  krpc::response::ping(ctx.out, ctx.transaction, dht.id);
} // ping::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  dht_response(ctx, sender, [](auto &node) { //
    node.ping_outstanding = 0;
  });
} // ping::handle_response()

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }

    handle_response(ctx, sender);
    return true;
  });
}

static bool
on_request(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }

    handle_request(ctx, sender);
    return true;
  });
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
  dht::DHT &dht = ctx.dht;
  dht_request(ctx, sender, [](auto &) {});
  constexpr std::size_t capacity = 8;

  const krpc::Transaction &t = ctx.transaction;
  dht::Node *result[capacity];
  dht::find_closest(dht, search, result, capacity);
  krpc::response::find_node(ctx.out, t, dht.id, (const dht::Node **)&result,
                            capacity);
} // find_node::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const sp::list<dht::Node> &contacts) noexcept {
  dht::DHT &dht = ctx.dht;

  dht_response(ctx, sender, [&](auto &) {
    for_each(contacts, [&](auto &contact) { //

      dht::insert(dht, contact);
    });

  });

} // find_node::handle_response()

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::DHT &dht = ctx.dht;

    dht::NodeId id;
    sp::list<dht::Node> &nodes = dht.contact_list;

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }
    if (!bencode::d::pair(p, "nodes", nodes)) {
      return false;
    }

    handle_response(ctx, id, nodes);
    return true;
  });
}

static bool
on_request(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::NodeId id;
    dht::NodeId target;

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }
    if (!bencode::d::pair(p, "target", target.id)) {
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
  dht_request(ctx, id, [](auto &) {});

  dht::Token token;
  dht::DHT &dht = ctx.dht;
  dht::mintToken(dht, ctx.remote.ip, token);

  const krpc::Transaction &t = ctx.transaction;
  const dht::Peer *result = lookup::lookup(dht, search, ctx.now);
  if (result) {
    // TODO
    krpc::response::get_peers(ctx.out, t, dht.id, token, result);
  } else {
    constexpr std::size_t capacity = 8;
    dht::Node *closest[capacity];
    dht::find_closest(dht, search, closest, capacity);
    krpc::response::get_peers(ctx.out, t, dht.id, token,
                              (const dht::Node **)closest, capacity);
  }
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &token,
                const sp::list<dht::Contact> &values) noexcept {
  /*
   * infohash lookup query found result, sender returns requested data.
   */
  dht_response(ctx, sender, [](auto &) { //
  });
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &token,
                const sp::list<dht::Node> &contacts) noexcept {
  /*
   * sender has no information for queried infohash, returns the closest
   * contacts.
   */
  dht::DHT &dht = ctx.dht;
  dht_request(ctx, sender, [&](auto &) {
    for_each(contacts, [&](auto &contact) { //
      dht::insert(dht, contact);
    });
  });
}

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::DHT &dht = ctx.dht;

    dht::NodeId id;
    dht::Token token;

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }

    if (!bencode::d::pair(p, "token", token.id)) {
      return false;
    }

    /*closes K nodes*/
    sp::list<dht::Node> &nodes = dht.contact_list;
    if (bencode::d::pair(p, "nodes", nodes)) {
      handle_response(ctx, id, token, nodes);
      return true;
    }

    sp::list<dht::Contact> &values = dht.value_list;
    if (bencode::d::pair(p, "values", values)) {
      handle_response(ctx, id, token, values);
      return true;
    }

    return false;
  });
}

static bool
on_request(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) {
    dht::NodeId id;
    dht::Infohash infohash;

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }
    if (!bencode::d::pair(p, "info_hash", infohash.id)) {
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

      lookup::insert(dht, infohash, peer);
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
    dht::NodeId id;
    bool implied_port = false;
    dht::Infohash infohash;
    Port port = 0;
    dht::Token token;

    // TODO support for instances that does have exactly this order
    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }
    if (!bencode::d::pair(p, "implied_port", implied_port)) {
      return false;
    }
    if (!bencode::d::pair(p, "info_hash", infohash.id)) {
      return false;
    }
    // TODO optional
    if (!bencode::d::pair(p, "port", port)) {
      return false;
    }
    if (!bencode::d::pair(p, "token", token.id)) {
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

  krpc::response::error(ctx.out, ctx.transaction, krpc::Error::method_unknown,
                        "unknown method");
  return true;
}
void
setup(dht::Module &module) noexcept {
  module.query = "";
  module.request = on_request;
  module.response = on_response;
}
} // namespace error
