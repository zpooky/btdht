#ifndef SP_MAINLINE_DHT_LOG_H
#define SP_MAINLINE_DHT_LOG_H

#include "shared.h"

namespace log {
/*log*/

namespace receive {
/*log::receive*/

namespace req {
/*log::receive::req*/
void
ping(dht::MessageContext &) noexcept;

void
find_node(dht::MessageContext &) noexcept;

void
get_peers(dht::MessageContext &) noexcept;

void
announce_peer(dht::MessageContext &) noexcept;

void
error(dht::MessageContext &) noexcept;

void
dump(dht::MessageContext &) noexcept;
} // namespace req

namespace res {
/* log::receive::res */
void
ping(dht::MessageContext &) noexcept;

void
find_node(dht::MessageContext &) noexcept;

void
get_peers(dht::MessageContext &) noexcept;

void
announce_peer(dht::MessageContext &) noexcept;

void
error(dht::MessageContext &) noexcept;

void
known_tx(dht::MessageContext &) noexcept;

void
unknown_tx(dht::MessageContext &) noexcept;

} // namespace res

namespace parse {
/* log::receive::parse */
void
error(dht::DHT &, const sp::Buffer &) noexcept;

} // namespace parse
} // namespace receive

namespace awake {
/* log::awake */
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
/* log::transmit */
void
ping(dht::DHT &, const Contact &, bool) noexcept;

void
find_node(dht::DHT &, const Contact &, bool) noexcept;

void
get_peers(dht::DHT &, const Contact &, bool) noexcept;

namespace error {
/* log::transmit::error */
void
mint_transaction(const dht::DHT &) noexcept;

void
udp(const dht::DHT &) noexcept;

void
ping_response_timeout(dht::DHT &) noexcept;

void
find_node_response_timeout(dht::DHT &) noexcept;

void
get_peers_response_timeout(dht::DHT &) noexcept;
} // namespace error
} // namespace transmit

namespace routing {
/*log::routing*/
void
split(const dht::DHT &, const dht::RoutingTable &,
      const dht::RoutingTable &) noexcept;
void
insert(const dht::DHT &, const dht::Node &) noexcept;
} // namespace routing

namespace peer_db {
/*log::peer_db*/
void
insert(const dht::DHT &, const dht::Infohash &, const Contact &) noexcept;
} // namespace peer_db

namespace search {

void
retire(const dht::DHT &, const dht::Search &) noexcept;

} // namespace search
} // namespace log

#endif
