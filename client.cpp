#include "client.h"

#include "Log.h"
#include "dht.h"
#include "dht_interface.h"
#include "krpc.h"
#include "udp.h"

namespace client {

template <typename F>
static bool
send(dht::DHT &dht, const Contact &remote, sp::Buffer &out, dht::Module module,
     void *closure, F request) noexcept {
  sp::reset(out);
  dht::Client &client = dht.client;

  bool result = false;
  krpc::Transaction tx;
  dht::TxContext ctx{module.response, module.response_timeout, closure};
  if (mint_tx(dht, tx, ctx)) {

    result = request(out, tx);
    if (result) {
      sp::flip(out);
      result = udp::send(client.udp, remote, out);
    }

    if (!result) {
      log::transmit::error::udp(dht);
      // since we fail to send request, we clear the transaction
      dht::TxContext dummy;
      assert(take_tx(client, tx, dummy));
    }
  } else {
    log::transmit::error::mint_transaction(dht);
  }

  return result;
}

bool
ping(dht::DHT &dht, sp::Buffer &buf, const dht::Node &node) noexcept {
  dht::Module ping;
  ping::setup(ping);

  auto serialize = [&dht, &node](sp::Buffer &out, const krpc::Transaction &t) {
    return krpc::request::ping(out, t, dht.id);
  };

  bool result = send(dht, node.contact, buf, ping, nullptr, serialize);
  log::transmit::ping(dht, node.contact, result); // TODO log tx
  return result;
}

bool
find_node(dht::DHT &dht, sp::Buffer &buf, const Contact &dest,
          const dht::NodeId &search, void *closure) noexcept {
  dht::Module find_node;
  find_node::setup(find_node);

  auto serialize = [&dht, &search](sp::Buffer &o, const krpc::Transaction &t) {
    return krpc::request::find_node(o, t, dht.id, search);
  };

  bool result = send(dht, dest, buf, find_node, closure, serialize);
  log::transmit::find_node(dht, dest, result); // TODO log tx
  return result;
}

} // namespace client
