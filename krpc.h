#ifndef SP_MAINLINE_DHT_KRPC_H
#define SP_MAINLINE_DHT_KRPC_H

#include "bencode_offset.h"
#include "shared.h"
#include <cassert>
#include <cstring>

namespace bencode {
namespace e {
// bool
// value(sp::Buffer &buffer, const dht::Contact &p) noexcept;
//
// bool
// value(sp::Buffer &buffer, const dht::Node &node) noexcept;

bool
pair_compact(sp::Buffer &, const char *, const dht::Contact *list) noexcept;

bool
pair_compact(sp::Buffer &, const char *, const sp::list<dht::Node> &list) noexcept;

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
krpc(ParseContext &ctx, F f) {
  return bencode::d::dict(ctx.decoder, [&ctx, f](auto &p) { //
    bool t = false;
    bool y = false;
    bool q = false;
    bool ip = false;
    bool v = false;

    ssize_t mark = -1;
    std::size_t mark_end = 0;

    char wkey[16] = {0};
    sp::byte wvalue[64] = {0};
  start:
    // TODO length compare for all raw indexing everywhere!!
    if (p.buf.raw[p.buf.pos] != 'e') {
      const std::size_t before = p.buf.pos;
      if (!t && bencode::d::pair(p, "t", ctx.tx.id)) {
        ctx.tx.length = std::strlen((char *)ctx.tx.id);
        t = true;
        goto start;
      } else {
        assert(before == p.buf.pos);
      }

      if (!y && bencode::d::pair(p, "y", ctx.msg_type)) {
        y = true;
        goto start;
      } else {
        assert(before == p.buf.pos);
      }

      if (!q && bencode::d::pair(p, "q", ctx.query)) {
        q = true;
        goto start;
      } else {
        assert(before == p.buf.pos);
      }

      if (!ip && bencode::d::pair(p, "ip", ctx.ext_ip)) {
        ip = true;
        goto start;
      } else {
        assert(before == p.buf.pos);
      }

      if (!v && bencode::d::pair(p, "v", ctx.remote_version)) {
        v = true;
        goto start;
      } else {
        assert(before == p.buf.pos);
      }

      // the application layer dict[request argument:a, reply: r]
      if (bencode::d::peek(p, "a") || bencode::d::peek(p, "r")) {
        mark = p.buf.pos;
        assert(bencode::d::value(p, "a") || bencode::d::value(p, "r"));

        if (!bencode::d::dict_wildcard(p)) {
          return false;
        }
        mark_end = p.buf.pos;

        goto start;
      } else {
        // parse and ignore unknown attributes for future compatability
        if (!bencode::d::pair_any(p, wkey, wvalue)) {
          return false;
        }
        goto start;
      }
    }

    auto is_query = [&]() { //
      return std::strcmp("q", ctx.msg_type) == 0;
    };
    auto is_reply = [&] { //
      return std::strcmp("r", ctx.msg_type) == 0;
    };

    if (is_query()) {
      if (!(t && y && q)) {
        return false;
      }
    } else if (is_reply()) {
      if (!(t && y)) {
        return false;
      }
    } else {
      return false;
    }

    if (mark < 0) {
      return false;
    }

    sp::Buffer copy(p.buf, mark, mark_end);
    bencode::d::Decoder dc(copy);
    krpc::ParseContext abbriged(ctx, dc);
    return f(abbriged);
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
