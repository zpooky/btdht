#include "client.h"
#include "dht.h"
#include "krpc.h"
#include "udp.h"

namespace client {

bool
ping(dht::DHT &ctx, sp::Buffer &out, const dht::Node &node) noexcept {
  sp::reset(out);

  dht::Client &client = ctx.client;
  krpc::Transaction t;
  mint_tx(client, t, ctx.now, nullptr); // TODO

  krpc::request::ping(out, t, ctx.id);
  sp::flip(out);
  bool result = udp::send(client.udp, node.contact, out);
  if (!result) {
    // since we fail to send request, we clear the transaction
    take_tx(client, t);
  }

  return result;
}
} // namespace client
