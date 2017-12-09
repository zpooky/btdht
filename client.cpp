#include "client.h"
#include "udp.h"

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
  // TODO
  return true;
}

/*TxTree*/
TxTree::TxTree() noexcept {
}

/*dht::Client*/
Client::Client(fd &fd) noexcept
    : udp(fd) {
}

bool
send(Client &c, const Contact &dest, const krpc::Transaction &,
     sp::Buffer &buf) noexcept {
  // TODO maintain transaction tree

  return udp::send(c.udp, dest, buf);
}

} // namespace dht
