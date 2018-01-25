#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"
#include "transaction.h"

// # Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {

void
randomize(NodeId &) noexcept;

bool
should_mark_bad(DHT &, Node &) noexcept;

bool
is_good(const DHT &, const Node &) noexcept;

bool
init(dht::DHT &) noexcept;

bool
is_blacklisted(DHT &dht, const Contact &) noexcept;

/**/
void
multiple_closest(DHT &, const NodeId &, Node *(&)[Bucket::K]) noexcept;

void
multiple_closest(DHT &, const Infohash &, Node *(&)[Bucket::K]) noexcept;

bool
valid(DHT &, const krpc::Transaction &) noexcept;

Node *
find_contact(DHT &, const NodeId &) noexcept;

Bucket *
bucket_for(DHT &, const NodeId &) noexcept;

Node *
insert(DHT &, const Node &) noexcept;

std::uint32_t
max_routing_nodes(DHT &) noexcept;

} // namespace dht



#endif
