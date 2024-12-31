#include "Log.h"

#include <cstring>
#include <inttypes.h>

#include <bencode_print.h>
#include <encode/hex.h>
#include <io/file.h>
#include <string/ascii.h>
#include <util.h>

// #define LOG_REQ_PING
// #define LOG_REQ_FIND_NODE
#define LOG_REQ_GET_PEERS
#define LOG_REQ_SAMPLE_INFOHASHES

// #define LOG_ROUTING_SPLIT
// #define LOG_ROUTING_INSERT

// #define LOG_ERROR_MINT_TX

#define LOG_RES_PING
#define LOG_RES_FIND_NODE
#define LOG_RES_GET_PEERS
#define LOG_RES_ANNOUNCE_PEER

#define LOG_PEER_DB

// #define LOG_AWAKE_TIMEOUT

// #define LOG_KNOWN_TX

namespace logger {
static void
print_raw(FILE *_f, const sp::byte *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    fprintf(_f, "%.*s", int(len), val);
  } else {
    fprintf(_f, "hex[");
    dht::print_hex(_f, (const sp::byte *)val, len);
    fprintf(_f, "]: %zu(", len);
    for (std::size_t i = 0; i < len; ++i) {
      if (ascii::is_printable(val[i])) {
        fprintf(_f, "%c", val[i]);
      } else {
        fprintf(_f, "_");
      }
    }
    fprintf(_f, ")");
  }
}

/*logger*/
static void
__print_time(FILE *f, const Timestamp &now) noexcept {
  char buff[32] = {0};

  sp::Seconds sec(sp::Milliseconds(now), sp::RoundMode::UP);
  time_t tim(sec);

  strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", localtime(&tim));
  fprintf(f, "%s|", buff);
}

static void
print_time(FILE *f, const dht::DHT &dht) noexcept {
  if (!dht.systemd) {
    __print_time(f, dht.now);
  }
}

static void
print_time(FILE *f, const dht::MessageContext &ctx) noexcept {
  print_time(f, ctx.dht);
}

// static void
// print_time(FILE *f, const dht::DHTMetaRoutingTable &ctx) noexcept {
//   __print_time(f, ctx.now);
// }

static void
print_time(FILE *f, const db::DHTMetaDatabase &ctx) noexcept {
  //TODO add if (!dht.systemd) {
  __print_time(f, ctx.now);
}

namespace receive {
/*logger::receive*/
namespace req {
/*logger::receive::req*/
void
ping(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.request.ping;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive request ping (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
}

void
find_node(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.request.find_node;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive request find_node (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
}

void
get_peers(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.received.request.get_peers;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive request get_peers (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
}

void
announce_peer(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.request.announce_peer;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive request announce_peer (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
}

void
error(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.request.error;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "UNKNOWN request %s (%s) <", ctx.query, to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);

  auto query_len = std::strlen(ctx.query);

  if (query_len < 127 && strncmp("vote", ctx.query, query_len) != 0) {
    if (ascii::is_printable(ctx.query, query_len)) {
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
        std::size_t wl;
        wl = fs::write(fd, "\t", 1);
        assertxs(wl == 1, wl, 1);

        const char *con = to_string(ctx.remote);
        wl = fs::write(fd, con, strlen(con));
        assertxs(wl == strlen(con), wl, strlen(con));

        size_t l_ver = strnlen((char *)ctx.pctx.remote_version,
                               sizeof(ctx.pctx.remote_version));
        if (l_ver > 0) {
          wl = fs::write(fd, "\t", 1);
          assertxs(wl == 1, wl, 1);

          wl = fs::write(fd, ctx.pctx.remote_version, l_ver);
          assertxs(wl == l_ver, wl, l_ver);
        }
        const unsigned char nl = '\n';
        wl = fs::write(fd, &nl, 1);
        assertxs(wl == 1, wl, 1);
      }
    }
  }
}

void
sample_infohashes(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.request.sample_infohashes;

  auto f = ctx.sample_infohashes;
  if (f && pctx.remote_version[0] != '\0') {
    // print_time(f, ctx);
    fprintf(f, "receive request sample_infohashes (%s) <",
            to_string(ctx.remote));
    dht::print_hex(f, tx.id, tx.length);
    fprintf(f, "> [%.*s]\n", 4, pctx.remote_version);
  }
}

void
dump(dht::MessageContext &ctx) noexcept {
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive dump\n");
}
} // namespace req

namespace res {
/*logger::receive::res*/
void
ping(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.response.ping;

#ifdef LOG_RES_PING
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive response ping      (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
#endif
}

void
find_node(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.response.find_node;

#ifdef LOG_RES_FIND_NODE
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive response find_node (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
#endif
}

void
get_peers(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.response.get_peers;

#ifdef LOG_RES_GET_PEERS
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive response get_peers (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
#endif
}

void
announce_peer(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.response.announce_peer;

#ifdef LOG_RES_ANNOUNCE_PEER
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "receive response announce_peer (%s) <", to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
#endif
}

void
error(dht::MessageContext &ctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  krpc::ParseContext &pctx = ctx.pctx;
  const krpc::Transaction &tx = ctx.transaction;

  ++s.received.response.error;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "UNKNOWN response %s (%s) <", ctx.query, to_string(ctx.remote));
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "> [%.*s]\n", 2, pctx.remote_version);
}

void
known_tx(dht::MessageContext &ctx, const tx::TxContext &tctx) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  sp::Milliseconds ms{tctx.latency};
  // TODO histogram tctx.latency
  ++s.known_tx;

#ifdef LOG_KNOWN_TX
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "known transaction[");
  auto &tx = ctx.transaction;
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "]\n");
#endif
}

void
unknown_tx(dht::MessageContext &ctx, const sp::Buffer &in) noexcept {
  dht::Stat &s = ctx.dht.statistics;
  ++s.unknown_tx;
  auto f = stderr;

  print_time(f, ctx);
  fprintf(f, "unknown transaction[");
  auto &tx = ctx.transaction;
  dht::print_hex(f, tx.id, tx.length);
  fprintf(f, "]\n");

  if (!bencode_print(in)) {
    hex::encode_print(in.raw, in.length, f);
  }
}
} // namespace res

namespace parse {
/*logger::receive::parse*/
void
error(dht::DHT &ctx, const sp::Buffer &buffer, const char *msg) noexcept {
  sp::Buffer copy(buffer);
  copy.pos = 0;

  dht::Stat &s = ctx.statistics;
  ++s.received.parse_error;

  auto f = stderr;
  print_time(f, ctx);
  fprintf(f, "parse error|%s|\n", msg);
  // dht::print_hex(copy.raw + copy.pos, copy.length);
  bencode_print_out(f);
  bencode_print(copy);
  bencode_print_out(stdout);
}

void
invalid_node_id(dht::MessageContext &ctx, const char *query,
                const sp::byte *version, std::size_t l_version,
                const dht::NodeId &id) noexcept {
  auto f = stderr;
  print_time(f, ctx);
  fprintf(f, "%s: invalid node id[", query);
  dht::print_hex(f, id.id, sizeof(id.id));
  fprintf(f, "] version[");
  print_raw(f, version, l_version);
  fprintf(f, "]\n");
}

void
self_sender(dht::MessageContext &ctx) noexcept {
}
} // namespace parse

} // namespace receive

namespace awake {
/*logger::awake*/
void
timeout(const dht::DHT &ctx, const Timestamp &timeout) noexcept {
#ifdef LOG_AWAKE_TIMEOUT
  auto f = stdout;
  print_time(f, ctx);
  Timestamp awake(timeout - ctx.now);
  fprintf(f, "awake next timeout[%" PRIu64 "ms] ", std::uint64_t(awake));
  __print_time(f, timeout);
  fprintf(f, "\n");
#else
  (void)ctx;
  (void)timeout;
#endif
}

void
contact_ping(const dht::DHT &ctx, const Timestamp &timeout) noexcept {
  auto f = stdout;
  print_time(f, ctx);
  // TODO fix better print
  Timestamp awake(timeout - ctx.now);
  fprintf(f, "awake contact_ping vote timeout[%" PRIu64 "ms] next date:",
          std::uint64_t(awake));
  __print_time(f, timeout);
  printf("\n");
}

void
peer_db(const dht::DHT &ctx, const Timestamp &timeout) noexcept {
#ifdef LOG_PEER_DB
  auto f = stdout;
  print_time(f, ctx);
  // TODO fix better print
  // printf("awake peer_db vote timeout[%" PRIu64 "ms] next date:",
  //        std::uint64_t(timeout));
  // print_time(stdout, ctx.timeout_peer_next);
  fprintf(f, "\n");
#endif
  (void)ctx;
  (void)timeout;
}

void
contact_scan(const dht::DHT &ctx) noexcept {
#if 0
  auto f = stdout;
  print_time(f,ctx);
  fprintf(f,"awake contact_scan\n");
#endif
  (void)ctx;
}

} // namespace awake

/*logger::transmit*/
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
  ++s.transmit.request.ping;

#ifdef LOG_REQ_PING
  auto f = stdout;
  print_time(f, ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  fprintf(f, "transmit ping[%s],res[%s],count[%zu]\n", remote,
          to_string(result), s.sent.request.ping);
#endif
  (void)contact;
  (void)result;
}

void
find_node(dht::DHT &ctx, const Contact &contact, client::Res result) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.request.find_node;

#ifdef LOG_REQ_FIND_NODE
  auto f = stdout;
  print_time(f, ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  // auto &tx = ctx.transaction;
  // dht::print_hex(tx.id, tx.length);

  fprintf(f, "transmit find_node[%s],res[%s],count[%zu]\n", remote,
          to_string(result), s.transmit.request.find_node);
#endif
  (void)contact;
  (void)result;
}

void
get_peers(dht::DHT &ctx, const Contact &contact, const dht::Infohash &search,
          client::Res result) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.request.get_peers;

#ifdef LOG_REQ_GET_PEERS
  auto f = stdout;
  print_time(f, ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  // auto &tx = ctx.transaction;
  // dht::print_hex(tx.id, tx.length);

  fprintf(f, "transmit get_peers[%s, ", remote);
  dht::print_hex(f, search.id, sizeof(search.id));
  fprintf(f, "],res[%s],count[%zu]\n", to_string(result),
          s.transmit.request.get_peers);
#else
  (void)contact;
  (void)search;
  (void)result;
#endif
}

void
sample_infohashes(dht::DHT &ctx, const Contact &contact, const dht::Key &id,
                  client::Res result) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.request.sample_infohashes;
#ifdef LOG_REQ_SAMPLE_INFOHASHES
  auto f = stdout;
  print_time(f, ctx);
  char remote[30] = {0};
  to_string(contact, remote, sizeof(remote));

  // auto &tx = ctx.transaction;
  // dht::print_hex(tx.id, tx.length);

  fprintf(f, "transmit sample_infohashes[%s, ", remote);
  dht::print_hex(f, id, sizeof(id));
  fprintf(f, "],res[%s],count[%zu]\n", to_string(result),
          s.transmit.request.sample_infohashes);
#else
  (void)contact;
  (void)id;
  (void)result;
#endif
}

/* logger::transmit::error */
void
error::mint_transaction(const dht::DHT &ctx) noexcept {
#ifdef LOG_ERROR_MINT_TX
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "\033[91mtransmit error mint_transaction\033[0m, acitve tx: %zu\n",
          ctx.client.active);
#endif
  (void)ctx;
}

void
error::udp(const dht::DHT &ctx) noexcept {
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "\033[91mtransmit error udp\033[0m\n");
}

static std::size_t tout = 0;

void
error::ping_response_timeout(dht::DHT &ctx, const krpc::Transaction &tx,
                             const Timestamp &sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.response_timeout.ping;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "\033[91mping response timeout\033[0m tx[");
  dht::print_hex(f, tx);
  fprintf(f, "] sent: ");
  __print_time(f, sent);
  fprintf(f, "#[%zu]\n", tout++);
}

void
error::find_node_response_timeout(dht::DHT &ctx, const krpc::Transaction &tx,
                                  const Timestamp &sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.response_timeout.find_node;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "\033[91mfind_node response timeout\033[0m tx[");
  dht::print_hex(f, tx);
  fprintf(f, "] sent: ");
  __print_time(f, sent);
  fprintf(f, "#[%zu]\n", tout++);
}

void
error::get_peers_response_timeout(dht::DHT &ctx, const krpc::Transaction &tx,
                                  const Timestamp &sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.response_timeout.get_peers;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "\033[91mget_peers response timeout\033[0m tx[");
  dht::print_hex(f, tx);
  fprintf(f, "] sent: ");
  __print_time(f, sent);
  fprintf(f, "#[%zu]\n", tout++);
}

void
error::sample_infohashes_response_timeout(dht::DHT &ctx,
                                          const krpc::Transaction &tx,
                                          const Timestamp &sent) noexcept {
  dht::Stat &s = ctx.statistics;
  ++s.transmit.response_timeout.sample_infohashes;

  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "\033[91msample_infohashes response timeout\033[0m tx[");
  dht::print_hex(f, tx);
  fprintf(f, "] sent: ");
  __print_time(f, sent);
  fprintf(f, "#[%zu]\n", tout++);
}

} // namespace transmit

/*logger::routing*/
void
routing::split(const dht::DHTMetaRoutingTable &ctx, const dht::RoutingTable &,
               const dht::RoutingTable &) noexcept {
#ifdef LOG_ROUTING_SPLIT
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "routing table split node\n");
#endif
  (void)ctx;
}

void
routing::insert(const dht::DHTMetaRoutingTable &ctx,
                const dht::Node &d) noexcept {
#ifdef LOG_ROUTING_INSERT
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "routing table insert nodeId[");
  dht::print_hex(f, d.id.id, sizeof(d.id.id));
  fprintf(f, "]\n");
#endif
  (void)ctx;
  (void)d;
}

void
routing::can_not_insert(const dht::DHTMetaRoutingTable &ctx,
                        const dht::Node &d) noexcept {
#ifdef LOG_ROUTING_CAN_NOT_INSERT
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "routing table can not insert nodeId[");
  dht::print_hex(f, d.id.id, sizeof(d.id.id));
  fprintf(f, "]\n");
#endif
  (void)ctx;
  (void)d;
}

void
routing::head_node(const dht::DHTMetaRoutingTable &rt, sp::Milliseconds x) {
  timeout::Timeout *timeout = rt.tb.timeout;
  if (timeout) {
    dht::Node *head = timeout->timeout_node;
    if (head) {
      char remote[30] = {0};
      auto f = stdout;
      __print_time(f, rt.now);
      to_string(head->contact, remote, sizeof(remote));
      fprintf(f, "Node[%s, ", remote);
      fprintf(f, "req_sent:");
      __print_time(f, head->req_sent);
      fprintf(f, ", wakeup:");
      __print_time(f, (head->req_sent + x));
      fprintf(f, "]\n");
    }
  }
}

void
peer_db::insert(const db::DHTMetaDatabase &ctx, const dht::Infohash &h,
                const Contact &) noexcept {
#ifdef LOG_PEER_DB
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "peer db insert infohash[");
  dht::print_hex(f, h.id, sizeof(h.id));
  fprintf(f, "]\n");
#endif
  (void)ctx;
  (void)h;
}

void
peer_db::update(const db::DHTMetaDatabase &ctx, const dht::Infohash &h,
                const dht::Peer &) noexcept {
#ifdef LOG_PEER_DB
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "peer db update infohash[");
  dht::print_hex(f, h.id, sizeof(h.id));
  fprintf(f, "]\n");
#endif
  (void)ctx;
  (void)h;
}

void
search::retire(const dht::DHT &ctx, const dht::Search &current) noexcept {
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "retire search[");
  dht::print_hex(f, current.search.id, sizeof(current.search.id));
  fprintf(f, "]\n");
}

void
spbt::publish(const dht::DHT &ctx, const dht::Infohash &ih, bool present) {
  auto f = stdout;
  print_time(f, ctx);
  fprintf(f, "spbt publish[");
  dht::print_hex(f, ih.id, sizeof(ih.id));
  fprintf(f, "] (present: %s)\n", present ? "TRUE" : "FALSE");
}

} // namespace logger
