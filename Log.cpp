#include "Log.h"
#include "cstdio"
#include <cstring>

namespace log {
/*log*/
static void
print_time(time_t now) noexcept {
  char buff[20] = {};
  strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
  printf("%s|", buff);
}

static void
print_time(const dht::DHT &dht) noexcept {
  return print_time(dht.now);
}

static void
print_time(const dht::MessageContext &ctx) noexcept {
  return print_time(ctx.dht);
}

static void
print_hex(const sp::byte *arr, std::size_t length) {
  const std::size_t hex_cap = 4096;
  char hexed[hex_cap + 1] = {0};

  std::size_t hex_length = 0;
  std::size_t i = 0;
  while (i < length && hex_length < hex_cap) {
    char buff[128];
    std::size_t buffLength = sprintf(buff, "%02x", arr[i++]);
    std::memcpy(hexed + hex_length, buff, buffLength);

    hex_length += buffLength;
  }

  if (i == length) {
    printf("%s", hexed);
  } else {
    printf("abbriged[%zu],hex[%zu]:%s", length, i, hexed);
  }
}

namespace receive {
/*log::receive*/
namespace req {
/*log::receive::req*/
void
ping(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive request ping\n");
}

void
find_node(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive request find_node\n");
}

void
get_peers(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive request get_peers\n");
}

void
announce_peer(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive request announce_peer\n");
}

void
error(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("unknow request query type %s\n", ctx.query);
}

void
dump(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive dump\n");
}
} // namespace req

namespace res {
/*log::receive::res*/
void
ping(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive response ping\n");
}

void
find_node(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive response find_node\n");
}

void
get_peers(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive response get_peers\n");
}

void
announce_peer(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive response announce_peer\n");
}

void
error(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("unknow response query type %s\n", ctx.query);
}

void
known_tx(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("known transaction[");
  auto &tx = ctx.transaction;
  print_hex(tx.id, tx.length);
  printf("]\n");
}

void
unknown_tx(const dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("unknow transaction[");
  auto &tx = ctx.transaction;
  print_hex(tx.id, tx.length);
  printf("]\n");
}
} // namespace res

namespace parse {
/*log::receive::parse*/
void
error(const dht::DHT &ctx, const sp::Buffer &buffer) noexcept {
  print_time(ctx);
  printf("parse error|");
  print_hex(buffer.raw + buffer.pos, buffer.length);
  printf("\n");
}
} // namespace parse

} // namespace receive

namespace awake {
/*log::awake*/
void
timeout(const dht::DHT &ctx, Timeout timeout) noexcept {
  print_time(ctx);
  printf("awake next timeout[%dms]\n", timeout);
}

void
contact_ping(const dht::DHT &ctx, Timeout timeout) noexcept {
  print_time(ctx);
  printf("awake contact_ping vote timeout[%dsec] next date:", timeout);
  print_time(ctx.timeout_next);
  printf("\n");
}

void
peer_db(const dht::DHT &ctx, Timeout timeout) noexcept {
  print_time(ctx);
  printf("awake peer_db vote timeout[%dsec] next date:", timeout);
  print_time(ctx.timeout_peer_next);
  printf("\n");
}

void
contact_scan(const dht::DHT &ctx) noexcept {
  print_time(ctx);
  printf("awake contact_scan\n");
}

} // namespace awake

namespace transmit {
/*log::transmit*/
void
ping(const dht::DHT &ctx, const Contact &contact, bool result) noexcept {
  print_time(ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  const char *status = result ? "\033[92mtrue\033[0m" : "\033[91mfalse\033[0m";
  printf("transmit ping[%s],res[%s]\n", remote, status);
}

void
find_node(const dht::DHT &ctx, const Contact &contact, bool result) noexcept {
  print_time(ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  // auto &tx = ctx.transaction;
  // print_hex(tx.id, tx.length);

  const char *status = result ? "\033[92mtrue\033[0m" : "\033[91mfalse\033[0m";
  printf("transmit find_node[%s],res[%s]\n", remote, status);
}

namespace error {
/* log::transmit::error */
void
mint_transaction(const dht::DHT &ctx) noexcept {
  print_time(ctx);
  printf("\033[91mtransmit error mint_transaction\033[0m, acitve tx: %zu\n",
         ctx.client.active);
}

void
udp(const dht::DHT &ctx) noexcept {
  print_time(ctx);
  printf("\033[91mtransmit error udp\n\033[0m");
}

} // namespace error

} // namespace transmit

namespace routing {
/*log::routing*/
void
split(const dht::DHT &ctx, const dht::RoutingTable &,
      const dht::RoutingTable &) noexcept {
  print_time(ctx);
  printf("routing table split node\n");
}

void
insert(const dht::DHT &ctx, const dht::Node &d) noexcept {
  print_time(ctx);
  printf("routing table insert nodeId[");
  print_hex(d.id.id, sizeof(d.id.id));
  printf("]\n");
}
} // namespace routing

namespace peer_db {
/*log::peer_db*/
void
insert(const dht::DHT &ctx, const dht::Infohash &h, const Contact &) noexcept {
  print_time(ctx);
  printf("peer db insert infohash[");
  print_hex(h.id, sizeof(h.id));
  printf("]\n");
}

} // namespace peer_db
} // namespace log
