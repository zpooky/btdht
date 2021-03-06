#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "shared.h"
#include "transaction.h"

// # Terminology
// - Peer implements Bittorrent protocol
// - Node implements Mainline DHT protocol

namespace dht {
void
debug_for_each(DHT &, void *, void (*)(void *, DHT &, const Node &)) noexcept;

std::size_t
debug_levels(const DHT &) noexcept;

bool
debug_assert_all(const DHT &);

bool
is_strict(const Ip &, const NodeId &) noexcept;

// void
// randomize(prng::xorshift32 &, NodeId &) noexcept;

bool
should_mark_bad(const DHT &, Node &) noexcept;

bool
is_good(const DHT &, const Node &) noexcept;

bool
init(DHT &) noexcept;

bool
is_blacklisted(const DHT &dht, const Contact &) noexcept;

/**/
void
multiple_closest(DHT &, const NodeId &, Node **result, std::size_t) noexcept;

template <std::size_t SIZE>
void
multiple_closest(DHT &dht, const NodeId &id, Node *(&result)[SIZE]) noexcept {
  return multiple_closest(dht, id, result, SIZE);
}

void
multiple_closest(DHT &, const Infohash &, Node **, std::size_t) noexcept;

template <std::size_t SIZE>
void
multiple_closest(DHT &dht, const Infohash &id, Node *(&result)[SIZE]) noexcept {
  return multiple_closest(dht, id, result, SIZE);
}

// bool
// valid(DHT &, const krpc::Transaction &) noexcept;

Node *
find_contact(DHT &, const NodeId &) noexcept;

const Bucket *
bucket_for(DHT &, const NodeId &) noexcept;

Node *
insert(DHT &, const Node &) noexcept;

std::uint32_t
max_routing_nodes(const DHT &) noexcept;

std::uint32_t
nodes_good(const DHT &) noexcept;

std::uint32_t
nodes_total(const DHT &) noexcept;

std::uint32_t
nodes_bad(const DHT &) noexcept;

//==========================================
} // namespace dht

#endif
