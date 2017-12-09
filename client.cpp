#include "client.h"
#include "udp.h"
#include <cstring>

namespace dht {
/*Tx*/
Tx::Tx() noexcept
    : handle(nullptr)
    , timeout_next(nullptr)
    , timeout_priv(nullptr)
    , sent(0)
    , prefix{0}
    , suffix{0} {
}

Tx::operator bool() const noexcept {
  return handle != nullptr;
}

/*TxTree*/
TxTree::TxTree() noexcept {
}

/*dht::Client*/
Client::Client(fd &fd) noexcept
    : udp(fd) {
}

Tx *
tx_for(Client &client, const krpc::Transaction &tx) noexcept {
  return nullptr;
}

bool
mint_transaction(Client &, krpc::Transaction &t, time_t now) noexcept {
  // TODO
  int r = rand();
  std::memcpy(t.id, &r, 2);
  t.length = 2;

  return true;
}

bool
send(Client &c, const Contact &dest, const krpc::Transaction &,
     sp::Buffer &buf) noexcept {
  // TODO need to know which fp
  // TODO maintain transaction tree

  return udp::send(c.udp, dest, buf);
}

} // namespace dht
