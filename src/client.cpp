#include "client.h"

#include "Log.h"
#include "dht.h"
#include "dht_interface.h"
#include "krpc.h"
#include "priv_krpc.h"
#include "udp.h"
#include <list/LinkedList.h>
#include <util/assert.h>

namespace client {
//=====================================
template <typename F>
static Res
send(dht::DHT &dht, const Contact &remote, sp::Buffer &out, F request,
     krpc::Transaction &tx) noexcept {
  sp::reset(out);

  dht::Client &client = dht.client;

  Res result = request(out, tx) ? Res::OK : Res::ERR;
  if (result == Res::OK) {
    sp::flip(out);
    result = udp::send(client.udp, remote, out) ? Res::OK : Res::ERR;
  }

  if (result != Res::OK) {
    logger::transmit::error::udp(dht);
    // since we fail to send request, we clear the transaction
    tx::TxContext dummy;
    if (!tx::consume_transaction(dht, tx, dummy)) {
      assertx(false);
    }
  }

  return result;
}

//=====================================
Res
ping(dht::DHT &dht, sp::Buffer &buf, const dht::Node &node) noexcept {
  Res result = Res::ERR_TOKEN;
  dht::Module ping;
  ping::setup(ping);

  auto serialize = [&dht](sp::Buffer &out, const krpc::Transaction &t) {
    return krpc::request::ping(out, t, dht.id);
  };

  krpc::Transaction tx;
  tx::TxContext ctx{ping.response, ping.response_timeout, nullptr};

  if (tx::mint_transaction(dht, /*OUT*/ tx, ctx)) {
    result = send(dht, node.contact, buf, serialize, tx);
    logger::transmit::ping(dht, node.contact, result); // TODO log tx
  } else {
    logger::transmit::error::mint_transaction(dht);
  }
  return result;
}

//=====================================
Res
find_node(dht::DHT &dht, sp::Buffer &buf, const Contact &dest,
          const dht::NodeId &search, void *closure) noexcept {
  Res result = Res::ERR_TOKEN;
  dht::Module find_node;
  find_node::setup(find_node);

  auto serialize = [&dht, &search](sp::Buffer &o, const krpc::Transaction &t) {
    bool n4 = true;
    bool n6 = false;
    return krpc::request::find_node(o, t, dht.id, search, n4, n6);
  };

  krpc::Transaction tx;
  tx::TxContext ctx{find_node.response, find_node.response_timeout, closure};

  if (tx::mint_transaction(dht, /*OUT*/ tx, ctx)) {
    result = send(dht, dest, buf, serialize, tx);
    logger::transmit::find_node(dht, dest, result); // TODO log tx
  } else {
    logger::transmit::error::mint_transaction(dht);
  }
  return result;
}

//=====================================
Res
get_peers(dht::DHT &dht, sp::Buffer &buf, const Contact &dest,
          const dht::Infohash &search, void *closure) noexcept {
  Res result = Res::ERR_TOKEN;
  dht::Module get_peers;
  get_peers::setup(get_peers);

  auto serialize = [&dht, &search](sp::Buffer &o, const krpc::Transaction &t) {
    bool n4 = true;
    bool n6 = false;
    return krpc::request::get_peers(o, t, dht.id, search, n4, n6);
  };

  krpc::Transaction tx;
  tx::TxContext ctx{get_peers.response, get_peers.response_timeout, closure};

  if (tx::mint_transaction(dht, /*OUT*/ tx, ctx)) {
    result = send(dht, dest, buf, serialize, tx);
    logger::transmit::get_peers(dht, dest, result); // TODO log tx
  } else {
    logger::transmit::error::mint_transaction(dht);
  }
  return result;
}

namespace priv {
//=====================================
template <typename Contacts>
Res
found(dht::DHT &dht, sp::Buffer &out, const dht::Infohash &search,
      const Contacts &contacts) noexcept {
  sp::reset(out);

  // TODO
  //- change from event to req and on resp we can mark sent contacts as
  //  consumed, otherwise the UDP packet might have got lost and all work is
  //  for not.
  //- need to train all results before search can be deleted
  Res result =
      krpc::priv::event::found(out, search, contacts) ? Res::OK : Res::ERR;
  if (result == Res::OK) {
    sp::flip(out);
    result = net::sock_write(dht.priv_fd, out) ? Res::OK : Res::ERR;
  }

  return result;
}

template Res
found<sp::LinkedList<Contact>>(dht::DHT &, sp::Buffer &, const dht::Infohash &,
                               const sp::LinkedList<Contact> &) noexcept;

} // namespace priv

//=====================================
} // namespace client
