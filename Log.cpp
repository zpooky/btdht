#include "Log.h"
#include "stdio.h"
#include <memory.h>

namespace log {

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
    memcpy(hexed + hex_length, buff, buffLength);

    hex_length += buffLength;
  }

  if (i == length) {
    printf("hex[%zu]:%s", i, hexed);
  } else {
    printf("abbriged[%zu],hex[%zu]:%s", length, i, hexed);
  }
}

namespace receive {
namespace req {

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
} // namespace req

namespace res {

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
} // namespace res

namespace parse {
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
void
ping(const dht::DHT &ctx, const dht::Contact &contact, bool result) noexcept {
  print_time(ctx);
  char remote[30] = {0};
  to_string(ExternalIp(contact.ip, contact.port), remote, sizeof(remote));
  printf("transmit ping[%s],res[%s]\n", remote, result ? "true" : "false");
}

void
find_node(const dht::DHT &ctx, const dht::Contact &contact,
          bool result) noexcept {
  print_time(ctx);
  char remote[30] = {0};
  to_string(ExternalIp(contact.ip, contact.port), remote, sizeof(remote));
  printf("transmit find_node[%s],res[%s]\n", remote, result ? "true" : "false");
}

} // namespace transmit

namespace routing {
void
split(const dht::DHT &ctx, const dht::RoutingTable &,
      const dht::RoutingTable &) noexcept {
  print_time(ctx);
  printf("routing table split node\n");
}
void
insert(const dht::DHT &ctx, const dht::Node &d) noexcept {
  print_time(ctx);
  printf("routing table insert node:");
  print_hex(d.id.id, sizeof(d.id.id));
  printf("\n");
}

} // namespace routing

namespace peer_db {
void
insert(const dht::DHT &ctx, const dht::Infohash &h,
       const dht::Contact &c) noexcept {
  print_time(ctx);
  printf("peer db insert infohash[");
  print_hex(h.id, sizeof(h.id));
  printf("]\n");
}
} // namespace peer_db

} // namespace log
