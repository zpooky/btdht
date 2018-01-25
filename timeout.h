#ifndef SP_MAINLINE_DHT_TIMEOUT_H
#define SP_MAINLINE_DHT_TIMEOUT_H

#include "shared.h"

namespace timeout {
void
unlink(dht::Node *&head, dht::Node *) noexcept;

void
unlink(dht::DHT &ctx, dht::Node *contact) noexcept;

void
unlink(dht::Peer *&head, dht::Peer *) noexcept;

void
append_all(dht::DHT &, dht::Node *) noexcept;

void
append_all(dht::DHT &, dht::Peer *) noexcept;
} // namespace timeout

#endif
