#ifndef SP_MAINLINE_DHT_LOG_H
#define SP_MAINLINE_DHT_LOG_H

#include "db.h"
#include "routing_table.h"
#include "shared.h"

// ===============================================================================
namespace logger {
/*logger*/

// ========================================
namespace receive {
/*logger::receive*/

// ====================
namespace req {
/*logger::receive::req*/
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
sample_infohashes(dht::MessageContext &) noexcept;

void
dump(dht::MessageContext &) noexcept;
} // namespace req

// ====================
namespace res {
/* logger::receive::res */
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
known_tx(dht::MessageContext &, const tx::TxContext &tctx) noexcept;

void
unknown_tx(dht::MessageContext &, const sp::Buffer &) noexcept;

} // namespace res

// ====================
namespace parse {
/* logger::receive::parse */
void
error(dht::DHT &, const sp::Buffer &, const char *msg) noexcept;

void
invalid_node_id(dht::MessageContext &ctx, const char *query,
                const sp::byte *version, std::size_t l_version,
                const dht::NodeId &id) noexcept;

void
self_sender(dht::MessageContext &ctx) noexcept;

} // namespace parse
// ====================

} // namespace receive

// ========================================
namespace awake {
/* logger::awake */
void
timeout(const dht::DHT &, const Timestamp &) noexcept;

void
contact_ping(const dht::DHT &, const Timestamp &) noexcept;

void
peer_db(const dht::DHT &, const Timestamp &) noexcept;

void
contact_scan(const dht::DHT &) noexcept;
} // namespace awake

// ========================================
namespace transmit {
/* logger::transmit */
void
ping(dht::DHT &, const Contact &, client::Res) noexcept;

void
find_node(dht::DHT &, const Contact &, client::Res) noexcept;

void
get_peers(dht::DHT &, const Contact &, client::Res) noexcept;

// ====================
namespace error {
/* logger::transmit::error */
void
mint_transaction(const dht::DHT &) noexcept;

void
udp(const dht::DHT &) noexcept;

void
ping_response_timeout(dht::DHT &, const krpc::Transaction &,
                      const Timestamp &) noexcept;

void
find_node_response_timeout(dht::DHT &, const krpc::Transaction &,
                           const Timestamp &) noexcept;

void
get_peers_response_timeout(dht::DHT &, const krpc::Transaction &,
                           const Timestamp &) noexcept;

void
sample_infohashes_response_timeout(dht::DHT &, const krpc::Transaction &,
                                   const Timestamp &) noexcept;
} // namespace error
// ====================
} // namespace transmit

// ========================================
namespace routing {
/*logger::routing*/
void
split(const dht::DHTMetaRoutingTable &, const dht::RoutingTable &,
      const dht::RoutingTable &) noexcept;
void
insert(const dht::DHTMetaRoutingTable &, const dht::Node &) noexcept;

void
can_not_insert(const dht::DHTMetaRoutingTable &, const dht::Node &) noexcept;
} // namespace routing

// ========================================
namespace peer_db {
/*logger::peer_db*/
void
insert(const db::DHTMetaDatabase &, const dht::Infohash &,
       const Contact &) noexcept;

void
update(const db::DHTMetaDatabase &, const dht::Infohash &,
       const dht::Peer &) noexcept;
} // namespace peer_db

// ========================================
namespace search {

void
retire(const dht::DHT &, const dht::Search &) noexcept;

} // namespace search
} // namespace logger
// ===============================================================================

#endif
