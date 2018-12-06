#include "client.h"

#include "Log.h"
#include "dht.h"
#include "dht_interface.h"
#include "krpc.h"
#include "udp.h"
#include <util/assert.h>

namespace client {

template <typename F>
static Res
send(dht::DHT &dht, const Contact &remote, sp::Buffer &out, dht::Module module,
     void *closure, F request) noexcept {
  sp::reset(out);
  dht::Client &client = dht.client;

  krpc::Transaction tx;
  tx::TxContext ctx{module.response, module.response_timeout, closure};
  Res result = tx::mint(dht, tx, ctx) ? Res::OK : Res::ERR_TOKEN;
  if (result == Res::OK) {

    result = request(out, tx) ? Res::OK : Res::ERR;
    if (result == Res::OK) {
      sp::flip(out);
      result = udp::send(client.udp, remote, out) ? Res::OK : Res::ERR;
    }

    if (result != Res::OK) {
      log::transmit::error::udp(dht);
      // since we fail to send request, we clear the transaction
      tx::TxContext dummy;
      if (!tx::consume(client, tx, dummy)) {
        assertx(false);
      }
    }
  } else {
    log::transmit::error::mint_transaction(dht);
  }

  return result;
}

Res
ping(dht::DHT &dht, sp::Buffer &buf, const dht::Node &node) noexcept {
  dht::Module ping;
  ping::setup(ping);

  auto serialize = [&dht, &node](sp::Buffer &out, const krpc::Transaction &t) {
    return krpc::request::ping(out, t, dht.id);
  };

  auto result = send(dht, node.contact, buf, ping, nullptr, serialize);
  log::transmit::ping(dht, node.contact, result); // TODO log tx
  return result;
}

Res
find_node(dht::DHT &dht, sp::Buffer &buf, const Contact &dest,
          const dht::NodeId &search, void *closure) noexcept {
  dht::Module find_node;
  find_node::setup(find_node);

  auto serialize = [&dht, &search](sp::Buffer &o, const krpc::Transaction &t) {
    return krpc::request::find_node(o, t, dht.id, search);
  };

  auto result = send(dht, dest, buf, find_node, closure, serialize);
  log::transmit::find_node(dht, dest, result); // TODO log tx
  return result;
}

Res
get_peers(dht::DHT &dht, sp::Buffer &buf, const Contact &dest,
          const dht::Infohash &search, void *closure) noexcept {
  dht::Module get_peers;
  get_peers::setup(get_peers);

  auto serialize = [&dht, &search](sp::Buffer &o, const krpc::Transaction &t) {
    return krpc::request::get_peers(o, t, dht.id, search);
  };

  auto result = send(dht, dest, buf, get_peers, closure, serialize);
  log::transmit::get_peers(dht, dest, result); // TODO log tx
  return result;
}

} // namespace client
