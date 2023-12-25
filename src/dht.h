#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"
#include "transaction.h"
#include "Options.h"

// # Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {

//==========================================

bool
is_strict(const Ip &, const NodeId &) noexcept;

bool
init(DHT &, const Options&) noexcept;

bool
is_good(const DHT &, const Node &) noexcept;

bool
is_blacklisted(const DHT &, const Contact &) noexcept;

bool
should_mark_bad(const DHT &self, Node &contact) noexcept;

} // namespace dht

#endif
