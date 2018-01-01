#ifndef SP_MAINLINE_DHT_LOG_H
#define SP_MAINLINE_DHT_LOG_H

#include "shared.h"

namespace log {
namespace receive {
namespace req {

void
ping(const dht::MessageContext &) noexcept;

void
find_node(const dht::MessageContext &) noexcept;

void
get_peers(const dht::MessageContext &) noexcept;

void
announce_peer(const dht::MessageContext &) noexcept;

void
error(const dht::MessageContext &) noexcept;
} // namespace req

namespace res {

void
ping(const dht::MessageContext &) noexcept;

void
find_node(const dht::MessageContext &) noexcept;

void
get_peers(const dht::MessageContext &) noexcept;

void
announce_peer(const dht::MessageContext &) noexcept;

void
error(const dht::MessageContext &) noexcept;

void
known_tx(const dht::MessageContext &) noexcept;

void
unknown_tx(const dht::MessageContext &) noexcept;
} // namespace res

namespace parse {
void
error(const dht::DHT &, const sp::Buffer &) noexcept;
}

} // namespace receive
namespace awake {
void
timeout(const dht::DHT &, Timeout) noexcept;

void
contact_ping(const dht::DHT &, Timeout) noexcept;

void
peer_db(const dht::DHT &, Timeout) noexcept;

void
contact_scan(const dht::DHT &) noexcept;
} // namespace awake

namespace transmit {
void
ping(const dht::DHT &, const dht::Contact &, bool) noexcept;

void
find_node(const dht::DHT &, const dht::Contact &, bool) noexcept;
} // namespace transmit

namespace routing {
void
split(const dht::DHT &, const dht::RoutingTable &,
      const dht::RoutingTable &) noexcept;
void
insert(const dht::DHT &, const dht::Node &) noexcept;
} // namespace routing

namespace peer_db {
void
insert(const dht::DHT &, const dht::Infohash &, const dht::Contact &) noexcept;
}

} // namespace log

#endif
