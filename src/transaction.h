#ifndef SP_MAINLINE_TRANSACTION_STUFF_H
#define SP_MAINLINE_TRANSACTION_STUFF_H

#include "shared.h"

namespace tx {
//=====================================
bool
init(dht::Client &) noexcept;

//=====================================
bool
consume(dht::Client &, const krpc::Transaction &,
        /*OUT*/ TxContext &) noexcept;

//=====================================
/* Keep tracks of active outgoing transactions and what module should handle the
 * response for an eventual response. Client maintains a tree of active
 * transactions together with function pointer for the module which should
 * handle the response.
 */
bool
mint(dht::DHT &, /*OUT*/ krpc::Transaction &, TxContext &) noexcept;

//=====================================
bool
is_valid(dht::DHT &, const krpc::Transaction &) noexcept;

//=====================================
Timestamp
next_available(const dht::DHT &) noexcept;

//=====================================
void
eager_tx_timeout(dht::DHT &, sp::Milliseconds) noexcept;

//=====================================
const Tx *
next_timeout(const dht::Client &) noexcept;

//=====================================
} // namespace tx

#endif
