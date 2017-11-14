#include "BEncode.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include "udp.h"
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

static Node *
take_timedout(DHT &ctx, time_t now) noexcept {
  Node *result = nullptr;
  Node *head = ctx.timeout_head;
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
increment_outstanding(Node *node) noexcept {
  auto &data = node->ping_outstanding;
  if (data != ~std::uint8_t(0)) {
    data += 1;
  }
}

static Node *
last(Node *node) noexcept {
Lstart:
  if (node) {
    node = node->next;
    goto Lstart;
  }
  return node;
}

static void
append(DHT &ctx, Node *node) noexcept {
  if (ctx.timeout_tail)
    ctx.timeout_tail->next = node;

  ctx.timeout_tail = last(node);
}

static Timeout
awake(DHT &ctx, fd &udp, sp::Buffer &out, time_t now) noexcept {
  if (ctx.timeout_next >= now) {
    // TODO assert out is empty
    Node *timedout = take_timedout(ctx, now);
    for_each(timedout, [&ctx, &udp, &out](Node *node) { //
      krpc::Transaction t;
      // TODO t
      krpc::request::ping(out, t, node->id);
      sp::flip(out);
      udp::send(udp, node->peer, out);
      increment_outstanding(node);
      // TODO
      // fake update activity otherwise if all nodes have to same timeout we
      // will spam out pings, ex: 3 noes timedout ,send ping, append, get the
      // next timeout date, since there is only 3 in the queue and we will
      // immediately awake and send ping  to the same 3 nodes
    });
    append(ctx, timedout);

    // TODO always have a queue of closes timeout when receive anything
    // we remove from list and add to tail of list
    //- have a tail pointer to append at
    //- have a front pointer to look at the front from
    //-when adding new node append to end
    //-when updating activity append to end
    //-when removing contact remove contact from queue
    //-when finding next timeout slot look at the front of timeout queue
    //-when sending ping dequeue them and append them at the end(even though we
    // do
    // not know if the contact is well yet) and increment to contact
    // awaiting-ping-response counter
  }
  return now - ctx.timeout_next;
}

} // namespace dht

// TODO change to more descriptive function names
// TODO check that peer owns nodeid before change anything
// TODO not having out buffer when receiveing a repsonse so we do not respond to
// responses
// TODO every received msg(request/response) should update the activity
// timestamp.
// TODO calculate latency by having a list of active transaction{id,time_sent}
// latency = now - time_sent. To be used an desiding factor when inserting to a
// full not exapndable bucket. or desiding on which X best Contact Nodes when
// returning find_node/get_peers

//===========================================================
// Ping
//===========================================================
namespace ping {
static void
handle_request(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  /*we receive a ping request?*/
  constexpr bool is_ping = true;

  time_t now = ctx.now;
  if (!dht::update_activity(ctx.dht, sender, now, is_ping)) {
    dht::Node contact(sender, ctx.remote, now);
    dht::add(ctx.dht, contact);
  }
  // TODO clear ctx.timeout_list

  krpc::response::ping(ctx.out, ctx.transaction, ctx.dht.id);
} // ping::handle_request()

static void
handle_response(dht::MessageContext &ctx, const dht::NodeId &sender) noexcept {
  // if (dht::valid_transaction(ctx, t)) {
  // }

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
handle_request(dht::MessageContext &ctx, const dht::NodeId &self,
               const dht::NodeId &search) noexcept {
  // TODO
}

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //

    dht::NodeId id;
    sp::list<dht::NodeId> *target = nullptr; // TODO

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }

    // TODO impl.res_find_node(ctx, id, target);
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
               const dht::Infohash &infohash) noexcept {
  // TODO
}

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::NodeId id;
    dht::Token token;
    sp::list<dht::Node> *values = nullptr; // TODO

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }

    if (!bencode::d::pair(p, "token", token.id)) {
      return false;
    }
    // const sp::list<dht::NodeId> *targets; // TODO

    // TODO impl.res_get_peers_contact(ctx, id, token, values);
    return true;
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
handle_request(dht::MessageContext &ctx, const dht::NodeId &id,
               bool implied_port, const dht::Infohash &infohash, Port port,
               const char *token) noexcept {
  // TODO
}

static bool
on_response(dht::MessageContext &ctx) {
  return bencode::d::dict(ctx.in, [&ctx](auto &p) { //
    dht::NodeId id;
    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }

    // TODO impl.res_announce(ctx, id);
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
