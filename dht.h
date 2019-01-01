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

bool
is_strict(const Ip &, const NodeId &) noexcept;

void
randomize(prng::xorshift32 &, NodeId &) noexcept;

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

bool
valid(DHT &, const krpc::Transaction &) noexcept;

Node *
find_contact(DHT &, const NodeId &) noexcept;

const Bucket *
bucket_for(DHT &, const NodeId &) noexcept;

Node *
insert(DHT &, const Node &) noexcept;

std::uint32_t
max_routing_nodes(DHT &) noexcept;

std::uint32_t
nodes_good(const DHT &) noexcept;

std::uint32_t
nodes_total(const DHT &) noexcept;

std::uint32_t
nodes_bad(const DHT &) noexcept;

//==========================================
void
bootstrap_insert(DHT &, const Contact &) noexcept;

void
bootstrap_insert(DHT &, sp::dstack_node<Contact> *) noexcept;

void
bootstrap_insert_force(DHT &, const Contact &) noexcept;

void
bootstrap_insert_force(DHT &, sp::dstack_node<Contact> *) noexcept;

void
bootstrap_reset(DHT &) noexcept;

//==========================================
} // namespace dht

#endif
