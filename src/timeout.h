#ifndef SP_MAINLINE_DHT_TIMEOUT_H
#define SP_MAINLINE_DHT_TIMEOUT_H

#include "shared.h"

namespace timeout {
//=====================================
std::size_t
debug_count_nodes(const dht::DHT &) noexcept;

//=====================================
const dht::Node *
debug_find_node(const dht::DHT &, const dht::Node *) noexcept;

//=====================================
void
unlink(dht::Node *&head, dht::Node *) noexcept;

void
unlink(dht::DHT &, dht::Node *) noexcept;

void
unlink(dht::DHT &, dht::Peer *) noexcept;

//=====================================
void
append_all(dht::DHT &, dht::Node *) noexcept;

void
append_all(dht::DHT &, dht::Peer *) noexcept;

//=====================================
void
prepend(dht::DHT &, dht::Node *) noexcept;

//=====================================
void
insert_new(dht::DHT &, dht::Node *) noexcept;

//=====================================
void
insert(dht::Peer *priv, dht::Peer *subject, dht::Peer *next) noexcept;

//=====================================
dht::Node *
take_node(dht::DHT &, sp::Milliseconds timeout) noexcept;

dht::Peer *
take_peer(dht::DHT &, sp::Milliseconds timeout) noexcept;

// TODO XXX what is timeout????!?!?! (i want last sent timeout)

//=====================================
} // namespace timeout

#endif
