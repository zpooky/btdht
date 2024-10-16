#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "Options.h"
#include "shared.h"
#include "transaction.h"

// # Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
bool
is_valid_strict_id(const Ip &, const NodeId &) noexcept;

bool
randomize_NodeId(prng::xorshift32 &r, const Ip &addr, NodeId &id) noexcept;

bool
init(DHT &, const Options &) noexcept;

bool
is_blacklisted(const DHT &, const Contact &) noexcept;

bool
should_mark_bad(const DHT &self, Node &contact) noexcept;

std::size_t
shared_prefix(const dht::Key &a, const dht::NodeId &b) noexcept;
std::size_t
shared_prefix(const dht::NodeId &a, const dht::NodeId &b) noexcept;

dht::Node *
dht_insert(DHT &self, const sp::byte *version, const Node &contact) noexcept;
} // namespace dht

#endif
