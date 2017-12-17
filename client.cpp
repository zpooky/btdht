#include "client.h"
#include "dht.h"
#include "krpc.h"
#include "module.h"
#include "udp.h"

namespace client {

bool
ping(dht::DHT &ctx, sp::Buffer &out, const dht::Node &node) noexcept {
  sp::reset(out);
  dht::Client &client = ctx.client;

  dht::Module module;
  ping::setup(module);

  bool result = false;
  krpc::Transaction t;
  if (mint_tx(client, t, ctx.now, module.response)) {

    krpc::request::ping(out, t, ctx.id);
    sp::flip(out);
    result = udp::send(client.udp, node.contact, out);
    if (!result) {
      // since we fail to send request, we clear the transaction
      take_tx(client, t);
    }
  }

  return result;
}
} // namespace client
