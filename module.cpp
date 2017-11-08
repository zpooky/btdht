#include "BEncode.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"

//===========================================================
// Module
//===========================================================
namespace dht {
Module::Module()
    : query(nullptr)
    , response(nullptr)
    , request(nullptr) {
}

} // namespace dht

//===========================================================
// Ping
//===========================================================
namespace ping {
namespace handle {

static void
request(dht::DHT &ctx, sp::Buffer &out, const dht::Peer &remote, //
        const dht::NodeId &sender) noexcept {
  time_t now = time(0);
  if (!dht::update_activity(ctx, sender, now)) {
    dht::Node contact(sender, remote.ip, remote.port, now);
    dht::add(ctx, contact);
  }

  krpc::response::ping(out, ctx.id);
} // ping::handle::request()

static void
response(dht::DHT &ctx, sp::Buffer &out, const dht::Peer &remote, //
         const dht::NodeId &sender) noexcept {
} // ping::handle::response()

} // namespace handle

static bool
response(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
         sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote, &out](auto &p) { //
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }

    handle::response(ctx, out, remote, sender);
    return true;
  });
}

static bool
request(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
        sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote, &out](auto &p) { //
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }

    handle::request(ctx, out, remote, sender);
    return true;
  });
}

void
setup(dht::Module &module) noexcept {
  module.query = "ping";
  module.request = request;
  module.response = response;
}
} // namespace ping

//===========================================================
// find_node
//===========================================================
namespace find_node {
namespace handle { //
static void
request(dht::DHT &ctx, sp::Buffer &out, const dht::Peer &, //
        const dht::NodeId &self, const dht::NodeId &search) noexcept {
  // TODO
}

} // namespace handle

static bool
response(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
         sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote](auto &p) { //

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
request(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
        sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote, &out](auto &p) { //
    dht::NodeId id;
    dht::NodeId target;

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }
    if (!bencode::d::pair(p, "target", target.id)) {
      return false;
    }

    handle::request(ctx, out, remote, id, target);
    return true;
  });
}

void
setup(dht::Module &module) noexcept {
  module.query = "find_node";
  module.request = request;
  module.response = response;
}

} // namespace find_node

//===========================================================
// get_peers
//===========================================================
namespace get_peers {
namespace handle { //
static void
request(dht::DHT &ctx, sp::Buffer &out, const dht::Peer &, //
        const dht::NodeId &id, const dht::Infohash &infohash) noexcept {
  // TODO
}

} // namespace handle

static bool
response(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
         sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote](auto &p) { //
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
request(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
        sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote, &out](auto &p) {
    dht::NodeId id;
    dht::Infohash infohash;

    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }
    if (!bencode::d::pair(p, "info_hash", infohash.id)) {
      return false;
    }

    handle::request(ctx, out, remote, id, infohash);
    return true;
  });
}

void
setup(dht::Module &module) noexcept {
  module.query = "get_peers";
  module.request = request;
  module.response = response;
}
} // namespace get_peers

//===========================================================
// announce_peer
//===========================================================
namespace announce_peer {
namespace handle { //
static void
request(dht::DHT &ctx, sp::Buffer &out, const dht::Peer &, //
        const dht::NodeId &id, bool implied_port, const dht::Infohash &infohash,
        Port port, const char *token) noexcept {
  // TODO
}

} // namespace handle

static bool
response(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
         sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &remote](auto &p) { //
    dht::NodeId id;
    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }

    // TODO impl.res_announce(ctx, id);
    return true;
  });
}

static bool
request(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
        sp::Buffer &out) {
  return bencode::d::dict(d, [&ctx, &out, &remote](auto &p) {
    dht::NodeId id;
    bool implied_port = false;
    dht::Infohash infohash;
    Port port = 0;
    char token[16] = {0};

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

    handle::request(ctx, out, remote, id, implied_port, infohash, port, token);
    return true;
  });
}

void
setup(dht::Module &module) noexcept {
  module.query = "announce_peer";
  module.request = request;
  module.response = response;
}
} // namespace announce_peer

//===========================================================
// error
//===========================================================
namespace error {
static bool
response(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
         sp::Buffer &out) {
  // TODO log
}

static bool
request(dht::DHT &ctx, const dht::Peer &remote, bencode::d::Decoder &d,
        sp::Buffer &out) {
  // TODO build error response
}
void
setup(dht::Module &module) noexcept {
  module.query = "";
  module.request = request;
  module.response = response;
}
} // namespace error
