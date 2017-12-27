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
} // namespace res

} // namespace receive
} // namespace log

#endif
