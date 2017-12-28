#ifndef SP_MAINLINE_TRANSACTION_STUFF_H
#define SP_MAINLINE_TRANSACTION_STUFF_H

#include "shared.h"

namespace dht {

bool
init(Client &) noexcept;

bool
take_tx(Client &, const krpc::Transaction &,
        /*OUT*/ TxContext &) noexcept;

/*
 * keep tracks of active outgoing transactions and what module should handle the
 * response for an eventual response. Client maintains a tree of active
 * transactions together with function pointer for the module which should
 * handle the response.
 */
bool
mint_tx(DHT &, /*OUT*/ krpc::Transaction &, TxContext &ctx) noexcept;

} // namespace dht

#endif
