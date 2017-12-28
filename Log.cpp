#include "Log.h"
#include "stdio.h"

namespace log {
namespace receive {
namespace req {

void
ping(const dht::MessageContext &) noexcept {
  printf("receive request ping\n");
}

void
find_node(const dht::MessageContext &) noexcept {
  printf("receive request find_node\n");
}

void
get_peers(const dht::MessageContext &) noexcept {
  printf("receive request get_peers\n");
}

void
announce_peer(const dht::MessageContext &) noexcept {
  printf("receive request announce_peer\n");
}

void
error(const dht::MessageContext &ctx) noexcept {
  printf("unknow request query type %s\n", ctx.query);
}
} // namespace req

namespace res {

void
ping(const dht::MessageContext &) noexcept {
  printf("receive response ping\n");
}

void
find_node(const dht::MessageContext &) noexcept {
  printf("receive response find_node\n");
}

void
get_peers(const dht::MessageContext &) noexcept {
  printf("receive response get_peers\n");
}

void
announce_peer(const dht::MessageContext &) noexcept {
  printf("receive response announce_peer\n");
}

void
error(const dht::MessageContext &ctx) noexcept {
  printf("unknow response query type %s\n", ctx.query);
}
} // namespace res

} // namespace receive
} // namespace log
