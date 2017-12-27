#include "Log.h"
#include "stdio.h"

namespace log {
namespace receive {
namespace req {

void
ping(const dht::MessageContext &) noexcept {
}

void
find_node(const dht::MessageContext &) noexcept {
}

void
get_peers(const dht::MessageContext &) noexcept {
}

void
announce_peer(const dht::MessageContext &) noexcept {
}

void
error(const dht::MessageContext &ctx) noexcept {
  printf("unknow request query type %s\n", ctx.query);
}
} // namespace req

namespace res {

void
ping(const dht::MessageContext &) noexcept {
}

void
find_node(const dht::MessageContext &) noexcept {
}

void
get_peers(const dht::MessageContext &) noexcept {
}

void
announce_peer(const dht::MessageContext &) noexcept {
}

void
error(const dht::MessageContext &ctx) noexcept {
  printf("unknow response query type %s\n", ctx.query);
}
} // namespace res

} // namespace receive
} // namespace log
