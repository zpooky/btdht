#include "bencode.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include "udp.h"
#include <cassert>
#include <cstring>

//===========================================================
// Module
//===========================================================
namespace dht {

static bool
random(krpc::Transaction &t) noexcept {
  const char *a = "aa";
  // TODO
  std::memcpy(t.id, a, 3);
  return true;
}

static Timeout
awake(dht::DHT &, fd &, sp::Buffer &, time_t) noexcept;

/*MessageContext*/
MessageContext::MessageContext(DHT &p_dht, bencode::d::Decoder &p_in,
                               sp::Buffer &p_out, const krpc::Transaction &p_t,
                               Peer p_remote, time_t p_now) noexcept
    : dht{p_dht}
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
    node = node->next;
    goto Lstart;
  }
}

static void
increment_outstanding(Node *node) noexcept {
  auto &data = node->ping_outstanding;
  if (data != ~std::uint8_t(0)) {
    data += 1;
  }
}

} // namespace dht

namespace timeout {

static dht::Node *
take(dht::DHT &ctx, time_t now) noexcept {
  dht::Node *result = nullptr;
  dht::Node *head = ctx.timeout_head;
Lstart:
  if (head) {
    if (head->activity <= now) {
      // iterate
      head = head->next;

      // build result
      head->next = result;
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
}

static void
update(dht::DHT &ctx, dht::Node *const contact, time_t now) noexcept {
  assert(now >= contact->activity);
  unlink(ctx, contact);
  contact->activity = now;
  append(ctx, contact);
}
} // namespace timeout

namespace dht {

static Timeout
awake(DHT &ctx, fd &udp, sp::Buffer &out, time_t now) noexcept {
  reset(out);
  if (ctx.timeout_next >= now) {
    Node *timedout = timeout::take(ctx, now);
    for_each(timedout, [&ctx, &udp, &out, now](Node *node) { //
      krpc::Transaction t;
      random(t);

      krpc::request::ping(out, t, node->id);
      sp::flip(out);
      udp::send(udp, node->peer, out);
      increment_outstanding(node);

      // fake update activity otherwise if all nodes have to same timeout we
      // will spam out pings, ex: 3 noes timed out ,send ping, append, get the
      // next timeout date, since there is only 3 in the queue and we will
      // immediately awake and send ping  to the same 3 nodes
      node->activity = now;
    });
    timeout::append(ctx, timedout);

    // TODO
    //-keep track of transaction ids we sent out in requests,bounded queue of
    // active transaction with a timout only send out pings based on how many
    // free slots in queue,delay the rest ready for ping the next 'awake' slot.
    //
    //-when finding next timeout slot look at the front of timeout queue
    //
    //-calc new timeout , shold be
    // max(next_timeout,config.get_min_timeout_granularity(60sec)),
    //
  }
  return now - ctx.timeout_next;
}

static Node *
update_activity(dht::MessageContext &ctx, const dht::NodeId &sender) {
  time_t now = ctx.now;
  DHT &dht = ctx.dht;

  Node *const contact = find_contact(dht, sender);
  if (contact) {
    timeout::update(dht, contact, now);

    return contact;
  }

  Node c(sender, ctx.remote, now);
  return dht::add(ctx.dht, c);
}

} // namespace dht

// TODO check that peer owns nodeid before change anything
// TODO calculate latency by having a list of active transaction{id,time_sent}
// latency = now - time_sent. To be used an deciding factor when inserting to a
// full not expandable bucket. Or deciding on which X best Contact Nodes when
// returning find_node/get_peers
// TODO lookup table values lifetime
// TODO krpc serialize + deserialize test case

//===========================================================
// Ping
//===========================================================
namespace ping {
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  dht::DHT &dht = ctx.dht;
  dht::update_activity(ctx, sender);

  krpc::response::ping(ctx.out, ctx.transaction, dht.id);
} // ping::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  dht::DHT &dht = ctx.dht;
  const krpc::Transaction &t = ctx.transaction;

  if (dht::valid(dht, t)) {
    dht::update_activity(ctx, sender);
  }
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
  dht::update_activity(ctx, sender);
  sp::list<dht::Node> result = dht::find_closest(dht, search, 8);
  krpc::response::find_node(ctx.out, ctx.transaction, dht.id, result);
} // find_node::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const sp::list<dht::Node> &contacts) noexcept {
  dht::DHT &dht = ctx.dht;
  const krpc::Transaction &t = ctx.transaction;

  if (dht::valid(dht, t)) {
    dht::update_activity(ctx, sender);
    for_each(contacts, [&dht, &ctx](auto &contact) { //
      contact.activity = ctx.now;
      contact.ping_outstanding = 0;
      dht::add(dht, contact);
    });
    // if (dht.bootstrap_mode) {
    //   for (node : nodes) {
    //     send_find_nodes(node);
    //   }
    // }
    // TODO
  }
} // find_node::handle_response()

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::DHT &dht = ctx.dht;

    dht::NodeId id;
    sp::list<dht::Node> nodes = dht.contact_list;

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
  dht::DHT &dht = ctx.dht;

  dht::update_activity(ctx, id);
  const dht::Peer *result = dht::lookup(dht, search);
  dht::Token token; // TODO
  if (result) {
    krpc::response::get_peers(ctx.out, ctx.transaction, dht.id, token, result);
  } else {
    sp::list<dht::Node> closest = dht::find_closest(dht, search, 8);
    krpc::response::get_peers(ctx.out, ctx.transaction, dht.id, token, closest);
  }
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &token,
                const sp::list<dht::Peer> &values) noexcept {
  dht::DHT &dht = ctx.dht;
  if (dht::valid(dht, ctx.transaction)) {
    dht::update_activity(ctx, sender);
    // TODO
  }
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender,
                const dht::Token &token,
                const sp::list<dht::Node> &contacts) noexcept {
  dht::DHT &dht = ctx.dht;
  if (dht::valid(dht, ctx.transaction)) {
    dht::update_activity(ctx, sender);
    for_each(contacts, [&dht, &ctx](auto &contact) { //
      contact.activity = ctx.now;
      contact.ping_outstanding = 0;
      dht::add(dht, contact);
    });
  }
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
    sp::list<dht::Node> nodes = dht.contact_list;
    if (bencode::d::pair(p, "nodes", nodes)) {
      handle_response(ctx, id, token, nodes);
      return true;
    }

    sp::list<dht::Peer> values = dht.value_list;
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
               const char *token) noexcept {
  // TODO
}

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  dht::DHT &dht = ctx.dht;
  if (dht::valid(dht, ctx.transaction)) {
    dht::update_activity(ctx, sender);
    // TODO
  }
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
    char token[16] = {0};

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
    if (!bencode::d::pair(p, "token", token)) {
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
  // TODO log
}

static bool
on_request(dht::MessageContext &ctx) {
  // TODO build error response
}
void
setup(dht::Module &module) noexcept {
  module.query = "";
  module.request = on_request;
  module.response = on_response;
}
} // namespace error
