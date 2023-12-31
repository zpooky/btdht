#ifndef SP_MAINLINE_TRANSACTION_STUFF_H
#define SP_MAINLINE_TRANSACTION_STUFF_H

#include "shared.h"

namespace tx {
/*
 * inactive transaction cones first in the list, then transactions which are
 * closest to timeout in order of sent timestamp
 */
//=====================================
bool
consume_transaction(dht::DHT &, const krpc::Transaction &,
                    /*OUT*/ TxContext &) noexcept;

//=====================================
bool
has_free_transaction(const dht::DHT &);

//=====================================
/* Keep tracks of active outgoing transactions and what module should handle the
 * response for an eventual response. Client maintains a tree of active
 * transactions together with function pointer for the module which should
 * handle the response.
 */
bool
mint_transaction(dht::DHT &, /*OUT*/ krpc::Transaction &, TxContext &) noexcept;

//=====================================
bool
is_valid(dht::DHT &, const krpc::Transaction &) noexcept;

//=====================================
Timestamp
next_available(const dht::DHT &) noexcept;

//=====================================
void
eager_tx_timeout(dht::DHT &) noexcept;

//=====================================
const Tx *
next_timeout(const dht::DHT &dht) noexcept;

//=====================================
} // namespace tx

#endif
