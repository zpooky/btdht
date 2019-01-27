#include "Log.h"
#include "cstdio"
#include <cstring>
#include <encode/hex.h>
#include <inttypes.h>
#include <io/file.h>
#include <string/ascii.h>

// #define LOG_REQ_PING
// #define LOG_REQ_FIND_NODE
#define LOG_REQ_GET_PEERS

// #define LOG_ROUTING_SPLIT
// #define LOG_ROUTING_INSERT

// #define LOG_ERROR_MINT_TX

// #define LOG_RES_PING
// #define LOG_RES_FIND_NODE
#define LOG_RES_GET_PEERS
#define LOG_RES_ANNOUNCE_PEER

// #define LOG_KNOWN_TX

namespace log {
/*log*/
static void
print_time(Timestamp now) noexcept {
  char buff[32] = {0};

  sp::Seconds sec(now, sp::RoundMode::UP);
  time_t tim(sec);

  strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", localtime(&tim));
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
  /* TODO convert to use hex::encode */
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

static void
print_hex(const krpc::Transaction &tx) {
  print_hex(tx.id, tx.length);
}

namespace receive {
/*log::receive*/
namespace req {
/*log::receive::req*/
void
ping(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.request.ping;

  print_time(ctx);
  printf("receive request ping\n");
}

void
find_node(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.request.find_node;

  print_time(ctx);
  printf("receive request find_node\n");
}

void
get_peers(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.request.get_peers;

  print_time(ctx);
  printf("receive request get_peers\n");
}

void
announce_peer(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.request.announce_peer;

  print_time(ctx);
  printf("receive request announce_peer\n");
}

void
error(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.request.error;

  print_time(ctx);
  printf("unknow request query type %s\n", ctx.query);

  auto query_len = std::strlen(ctx.query);

  if (query_len < 127) {
    if (ascii::is_alpha(ctx.query, query_len)) {
      char path[256] = {'\0'};
      sprintf(path, "./unknown_%s.txt", ctx.query);

      auto fd = fs::open_append(path);
      if (fd) {
        const auto &in = ctx.in;
        char buf[16] = {0};

        const std::uint8_t *it = in.raw;
        const std::uint8_t *const in_end = it + in.length;

        /* Convert to hex and write to file */
        while (it != in_end) {
          std::size_t buf_len = sizeof(buf);
          it = hex::encode_inc(it, in_end, buf, /*IN/OUT*/ buf_len);
          assertxs(buf_len <= sizeof(buf), buf_len, sizeof(buf));
          std::size_t wl = fs::write(fd, (unsigned char *)buf, buf_len);
          assertxs(wl == buf_len, wl, buf_len);
        }

        const unsigned char nl = '\n';
        std::size_t wl = fs::write(fd, &nl, 1);
        assertxs(wl == 1, wl, 1);
      }
    }
  }
}

void
dump(dht::MessageContext &ctx) noexcept {
  print_time(ctx);
  printf("receive dump\n");
}
} // namespace req

namespace res {
/*log::receive::res*/
void
ping(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.response.ping;

#ifdef LOG_RES_PING
  print_time(ctx);
  printf("receive response ping\n");
#endif
}

void
find_node(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.response.find_node;

#ifdef LOG_RES_FIND_NODE
  print_time(ctx);
  printf("receive response find_node\n");
#endif
}

void
get_peers(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.response.get_peers;

#ifdef LOG_RES_GET_PEERS
  print_time(ctx);
  printf("receive response get_peers\n");
#endif
}

void
announce_peer(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.response.announce_peer;

#ifdef LOG_RES_ANNOUNCE_PEER
  print_time(ctx);
  printf("receive response announce_peer\n");
#endif
}

void
error(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.response.error;

  print_time(ctx);
  printf("unknow response query type %s\n", ctx.query);
}

void
known_tx(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.known_tx;

#ifdef LOG_KNOWN_TX
  print_time(ctx);
  printf("known transaction[");
  auto &tx = ctx.transaction;
  print_hex(tx.id, tx.length);
  printf("]\n");
#endif
}

void
unknown_tx(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.unknown_tx;

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
error(dht::DHT &ctx, const sp::Buffer &buffer) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.received.parse_error;

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
  printf("awake next timeout[%" PRIu64 "ms] ", std::uint64_t(timeout));
  print_time(ctx.now + timeout);
  printf("\n");
}

void
contact_ping(const dht::DHT &ctx, Timeout timeout) noexcept {
  print_time(ctx);
  // TODO fix better print
  printf("awake contact_ping vote timeout[%" PRIu64 "ms] next date:",
         std::uint64_t(timeout));
  print_time(ctx.timeout_next);
  printf("\n");
}

void
peer_db(const dht::DHT &ctx, Timeout timeout) noexcept {
  print_time(ctx);
  // TODO fix better print
  printf("awake peer_db vote timeout[%" PRIu64 "ms] next date:",
         std::uint64_t(timeout));
  print_time(ctx.timeout_peer_next);
  printf("\n");
}

void
contact_scan(const dht::DHT &ctx) noexcept {
#if 0
  print_time(ctx);
  printf("awake contact_scan\n");
#endif
  (void)ctx;
}

} // namespace awake

/*log::transmit*/
namespace transmit {
static const char *
to_string(client::Res result) noexcept {
  switch (result) {
  case client::Res::OK:
    return "\033[92mOK\033[0m";
  case client::Res::ERR:
    return "\033[91mERR\033[0m";
  case client::Res::ERR_TOKEN:
    return "\033[91mERR_TOKEN\033[0m";
  }

  return "";
}

void
ping(dht::DHT &ctx, const Contact &contact, client::Res result) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.sent.request.ping;

#ifdef LOG_REQ_PING
  print_time(ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  printf("transmit ping[%s],res[%s],count[%zu]\n", remote, to_string(result),
         s.sent.request.ping);
#endif
  (void)contact;
  (void)result;
}

void
find_node(dht::DHT &ctx, const Contact &contact, client::Res result) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.sent.request.find_node;

#ifdef LOG_REQ_FIND_NODE
  print_time(ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  // auto &tx = ctx.transaction;
  // print_hex(tx.id, tx.length);

  printf("transmit find_node[%s],res[%s],count[%zu]\n", remote,
         to_string(result), s.sent.request.find_node);
#endif
  (void)contact;
  (void)result;
}

void
get_peers(dht::DHT &ctx, const Contact &contact, client::Res result) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.sent.request.get_peers;

#ifdef LOG_REQ_GET_PEERS
  print_time(ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  // auto &tx = ctx.transaction;
  // print_hex(tx.id, tx.length);

  printf("transmit get_peers[%s],res[%s],count[%zu]\n", remote,
         to_string(result), s.sent.request.get_peers);
#endif
}

namespace error {
/* log::transmit::error */
void
mint_transaction(const dht::DHT &ctx) noexcept {
#ifdef LOG_ERROR_MINT_TX
  print_time(ctx);
  printf("\033[91mtransmit error mint_transaction\033[0m, acitve tx: %zu\n",
         ctx.client.active);
#endif
  (void)ctx;
}

void
udp(const dht::DHT &ctx) noexcept {
  print_time(ctx);
  printf("\033[91mtransmit error udp\033[0m\n");
}

static std::size_t tout = 0;

void
ping_response_timeout(dht::DHT &ctx, const krpc::Transaction &tx,
                      Timestamp sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.sent.response_timeout.ping;

  print_time(ctx);
  printf("\033[91mping response timeout\033[0m transaction[");
  print_hex(tx);
  printf("] sent: ");
  print_time(sent);
  printf("seq[%zu]\n", tout++);
}

void
find_node_response_timeout(dht::DHT &ctx, const krpc::Transaction &tx,
                           Timestamp sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.sent.response_timeout.find_node;

  print_time(ctx);
  printf("\033[91mfind_node response timeout\033[0m transaction[");
  print_hex(tx);
  printf("] sent: ");
  print_time(sent);
  printf("seq[%zu]\n", tout++);
}

void
get_peers_response_timeout(dht::DHT &ctx, const krpc::Transaction &tx,
                           Timestamp sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.sent.response_timeout.get_peers;

  print_time(ctx);
  printf("\033[91mget_peers response timeout\033[0m transaction[");
  print_hex(tx);
  printf("] sent: ");
  print_time(sent);
  printf("seq[%zu]\n", tout++);
}

} // namespace error

} // namespace transmit

namespace routing {
/*log::routing*/
void
split(const dht::DHT &ctx, const dht::RoutingTable &,
      const dht::RoutingTable &) noexcept {
#ifdef LOG_ROUTING_SPLIT
  print_time(ctx);
  printf("routing table split node\n");
#endif
  (void)ctx;
}

void
insert(const dht::DHT &ctx, const dht::Node &d) noexcept {
#ifdef LOG_ROUTING_INSERT
  print_time(ctx);
  printf("routing table insert nodeId[");
  print_hex(d.id.id, sizeof(d.id.id));
  printf("]\n");
#endif
  (void)ctx;
  (void)d;
}

void
can_not_insert(const dht::DHT &ctx, const dht::Node &d) noexcept {
#ifdef LOG_ROUTING_CAN_NOT_INSERT
  print_time(ctx);
  printf("routing table can not insert nodeId[");
  print_hex(d.id.id, sizeof(d.id.id));
  printf("]\n");
#endif
  (void)ctx;
  (void)d;
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

namespace search {

void
retire(const dht::DHT &ctx, const dht::Search &current) noexcept {
  print_time(ctx);
  printf("retire search[");
  print_hex(current.search.id, sizeof(current.search.id));
  printf("]\n");
}

} // namespace search

} // namespace log
