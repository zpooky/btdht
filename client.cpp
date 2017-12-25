#include "client.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include "udp.h"

namespace client {

template <typename F>
static bool
send(dht::DHT &ctx, const dht::Contact &remote, sp::Buffer &out,
     dht::Module module, F request) noexcept {
  sp::reset(out);
  dht::Client &client = ctx.client;

  bool result = false;
  krpc::Transaction t;
  if (mint_tx(ctx, t, ctx.now, module.response, module.response_timeout)) {

    result = request(out, t);
    if (result) {
      sp::flip(out);
      result = udp::send(client.udp, remote, out);
    }
    if (!result) {
      // since we fail to send request, we clear the transaction
      take_tx(client, t);
    }
  }

  return result;
}

bool
ping(dht::DHT &ctx, sp::Buffer &b, const dht::Node &node) noexcept {
  dht::Module ping;
  ping::setup(ping);

  auto serialize = [&ctx, &node](sp::Buffer &out, const krpc::Transaction &t) {
    return krpc::request::ping(out, t, ctx.id);
  };

  return send(ctx, node.contact, b, ping, serialize);
}

bool
find_node(dht::DHT &ctx, sp::Buffer &b, const dht::Contact &dest,
          const dht::NodeId &search) noexcept {
  dht::Module find_node;
  find_node::setup(find_node);

  auto serialize = [&ctx, &search](sp::Buffer &o, const krpc::Transaction &t) {
    return krpc::request::find_node(o, t, ctx.id, search);
  };

  return send(ctx, dest, b, find_node, serialize);
}

} // namespace client
