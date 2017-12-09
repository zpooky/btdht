#ifndef SP_MAINLINE_CLIENT_H
#define SP_MAINLINE_CLIENT_H

#include "shared.h"

namespace dht {

struct MessageContext;

/*dht::TxStore*/
struct Tx {
  bool (*handle)(MessageContext *) noexcept;

  Tx *timeout_next;
  Tx *timeout_priv;
  // TODO

  time_t sent;

  sp::byte prefix[2];
  sp::byte suffix[2];

  Tx() noexcept;

  explicit operator bool() const noexcept;
};

/*dht::TxTree*/
struct TxTree {
  TxTree() noexcept;
};

/*dht::Client*/
struct Client {
  fd &udp;
  TxTree tree;

  explicit Client(fd &) noexcept;
};

Tx *
tx_for(Client &, const krpc::Transaction &) noexcept;

bool
mint_transaction(Client &, krpc::Transaction &, time_t) noexcept;

/*
 * keep tracks of active outgoing transactions and what module should handle the
 * response for an eventual response. Client maintains a tree of active
 * transactions together with function pointer for the module which should
 * handle the response.
 */
bool
send(Client &, const Contact &, const krpc::Transaction &,
     sp::Buffer &) noexcept;

} // namespace dht

#endif
