#ifndef SP_MAINLINE_CLIENT_H
#define SP_MAINLINE_CLIENT_H

#include "shared.h"

// TODO rename client to transaction
namespace dht {

struct MessageContext;

using TxHandle = bool (*)(MessageContext &) noexcept;

/*dht::TxStore*/
struct Tx {
  TxHandle handle;

  Tx *timeout_next;
  Tx *timeout_priv;

  time_t sent;

  sp::byte prefix[2];
  sp::byte suffix[2];

  Tx() noexcept;

  bool
  operator==(const krpc::Transaction &) const noexcept;

  int
  cmp(const krpc::Transaction &) const noexcept;
};

/*dht::TxTree*/
struct TxTree {
  static constexpr std::size_t levels = 7;
  Tx storage[127];

  TxTree() noexcept;
};

/*dht::Client*/
struct Client {
  fd &udp;
  TxTree tree;
  Tx *timeout_head;

  explicit Client(fd &) noexcept;
};

bool
init(Client &) noexcept;

TxHandle
take_tx(Client &, const krpc::Transaction &) noexcept;

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
