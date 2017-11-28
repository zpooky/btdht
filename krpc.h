#ifndef SP_MAINLINE_DHT_KRPC_H
#define SP_MAINLINE_DHT_KRPC_H

#include "shared.h"
#include <cassert>
#include <cstring>

namespace bencode {
namespace e {
bool
value(sp::Buffer &buffer, const dht::Contact &p) noexcept;

bool
value(sp::Buffer &buffer, const dht::Node &node) noexcept;

bool
pair(sp::Buffer &, const char *, const dht::Contact *list) noexcept;

bool
pair(sp::Buffer &, const char *, const sp::list<dht::Node> &list) noexcept;

} // namespace e
} // namespace bencode

namespace krpc {
enum class MessageType : char { query = 'q', response = 'r', error = 'e' };
enum class Query { ping, find_node, get_peers, announce_peer };
enum class Error {
  generic_error = 201,
  server_error = 202,
  protocol_error = 203,
  method_unknown = 204
};

namespace request {
bool
ping(sp::Buffer &, const Transaction &, //
     const dht::NodeId &sender) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, //
          const dht::NodeId &self, const dht::NodeId &search) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &id, const dht::Infohash &infohash) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, //
              const dht::NodeId &id, bool implied_port,
              const dht::Infohash &infohash, std::uint16_t port,
              const char *token) noexcept;
} // namespace request

namespace response {
bool
ping(sp::Buffer &, const Transaction &, //
     const dht::NodeId &receiver) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, //
          const dht::NodeId &, const dht::Node **target, std::size_t) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &id, const dht::Token &, const dht::Node **,
          std::size_t) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &id, const dht::Token &,
          const dht::Peer *) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, //
              const dht::NodeId &) noexcept;

bool
error(sp::Buffer &, const Transaction &, Error, const char *) noexcept;
} // namespace response

namespace d {
template <typename F>
bool
krpc(bencode::d::Decoder &d, Transaction &tx, char (&msg_type)[16],
     char (&query)[16], F f) {
  return bencode::d::dict(d, [&tx, &msg_type, &query, f](auto &p) { //
    bool t = false;
    bool y = false;
    bool q = false;
    bool ip = false;
    bool v = false;

    sp::byte version[16] = {0};
    sp::byte extIp[16] = {0};

    char wkey[16] = {0};
    sp::byte wvalue[16] = {0};
  start:
    const std::size_t before = p.buf.pos;
    if (!t && bencode::d::pair(p, "t", tx.id)) {
      t = true;
      goto start;
    } else {
      assert(before == p.buf.pos);
    }

    if (!y && bencode::d::pair(p, "y", msg_type)) {
      y = true;
      goto start;
    } else {
      assert(before == p.buf.pos);
    }

    if (!q && bencode::d::pair(p, "q", query)) {
      q = true;
      goto start;
    } else {
      assert(before == p.buf.pos);
    }

    if (!ip && bencode::d::pair(p, "ip", extIp)) {
      ip = true;
      goto start;
    } else {
      assert(before == p.buf.pos);
    }

    if (!v && bencode::d::pair(p, "v", version)) {
      v = true;
      goto start;
    } else {
      assert(before == p.buf.pos);
    }

    // bencode::d::pair
    // TODO query is optional for response

    if (std::strcmp("q", query) == 0) {
      if (!(t && y && q)) {
        return false;
      }
    } else {
      if (!(t && y)) {
        return false;
      }
    }

    return f(p, tx, msg_type, query);
  });
}
namespace response {

/*f: MessageContext -> NodeId -> bool*/
template <typename Ctx, typename F>
bool
ping(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) {
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }

    return f(ctx, sender);
  });
}

} // namespace response

namespace request {
/*f: MessageContext -> NodeId -> bool*/
template <typename Ctx, typename F>
bool
ping(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) { //
    dht::NodeId sender;
    if (!bencode::d::pair(p, "id", sender.id)) {
      return false;
    }

    return f(ctx, sender);
  });
}
} // namespace request

} // namespace d
} // namespace krpc

#endif
