#ifndef SP_MAINLINE_DHT_KRPC_H
#define SP_MAINLINE_DHT_KRPC_H

#include "bencode_offset.h"
#include "bencode_print.h"
#include "shared.h"
#include <cassert>
#include <cstring>

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
/*krpc::request*/
/* DHT interface */
//{
bool
ping(sp::Buffer &, const Transaction &, //
     const dht::NodeId &sender) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, //
          const dht::NodeId &self, const dht::NodeId &search) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &self, const dht::Infohash &search) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, //
              const dht::NodeId &self, bool implied_port,
              const dht::Infohash &search, Port port,
              const dht::Token &) noexcept;
//}

/*private interface*/
//{
bool
dump(sp::Buffer &b, const Transaction &) noexcept;

bool
statistics(sp::Buffer &b, const Transaction &t) noexcept;

bool
search(sp::Buffer &b, const Transaction &, const dht::Infohash &,
       std::size_t) noexcept;
//}
} // namespace request

namespace response {
/*krpc::response*/
/* DHT interface */
//{
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
//}

/*private interface*/
//{
bool
dump(sp::Buffer &b, const Transaction &t, const dht::DHT &) noexcept;

bool
statistics(sp::Buffer &b, const Transaction &t, const dht::Stat &) noexcept;

bool
search(sp::Buffer &b, const Transaction &t) noexcept;
//}

} // namespace response

namespace d {

template <typename F>
bool
krpc(ParseContext &ctx, F f) {
  return bencode::d::dict(ctx.decoder, [&ctx, f](auto &p) { //
    bool t = false;
    bool y = false;
    bool q = false;
    bool ip_handled = false;
    bool v = false;

    ssize_t mark = -1;
    std::size_t mark_end = 0;

    char wkey[128] = {0};
    sp::byte wvalue[128] = {0};
  start:
    // TODO length compare for all raw indexing everywhere!!
    if (p.raw[p.pos] != 'e') {
      const std::size_t before = p.pos;
      krpc::Transaction &tx = ctx.tx;
      if (!t && bencode::d::pair(p, "t", tx.id, tx.length)) {
        t = true;
        goto start;
      } else {
        assert(before == p.pos);
      }

      if (!y && bencode::d::pair(p, "y", ctx.msg_type)) {
        y = true;
        goto start;
      } else {
        assert(before == p.pos);
      }

      if (!q && bencode::d::pair(p, "q", ctx.query)) {
        q = true;
        goto start;
      } else {
        assert(before == p.pos);
      }

      {
        Contact ip;
        if (!ip_handled && bencode::d::pair(p, "ip", ip)) {
          ctx.ip_vote = ip;
          assert(bool(ctx.ip_vote));
          ip_handled = true;
          goto start;
        } else {
          assert(before == p.pos);
        }
      }

      if (!v && bencode::d::pair(p, "v", ctx.remote_version)) {
        v = true;
        goto start;
      } else {
        assert(before == p.pos);
      }

      // the application layer dict[request argument:a, reply: r]
      if (bencode::d::peek(p, "a") || bencode::d::peek(p, "r")) {
        mark = p.pos;
        assert(bencode::d::value(p, "a") || bencode::d::value(p, "r"));

        if (!bencode::d::dict_wildcard(p)) {
          return false;
        }
        mark_end = p.pos;

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
    auto is_error = [&] { //
      return std::strcmp("e", ctx.msg_type) == 0;
    };

    if (is_error()) {
      if (!(t)) {
        return false;
      }
    } else if (is_query()) {
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

    sp::Buffer copy(p, mark, mark_end);
    krpc::ParseContext abbriged(ctx, copy);
    return f(abbriged);
  });
}

namespace response {

/*f: MessageContext -> NodeId -> bool*/
template <typename Ctx, typename F>
bool
ping(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) {
    bool b_id = false;
    bool b_ip = false;

    dht::NodeId sender;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", sender.id)) {
      b_id = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assert(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (b_id) {
      return f(ctx, sender);
    }

    return false;
  });
}

} // namespace response

namespace request {
/*f: MessageContext -> NodeId -> bool*/
template <typename Ctx, typename F>
bool
ping(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) { //
    bool b_id = false;
    bool b_ip = false;

    dht::NodeId sender;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", sender.id)) {
      b_id = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assert(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (b_id) {
      return f(ctx, sender);
    }

    return false;
  });
}

template <typename Ctx, typename F>
bool
search(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) {
    dht::Infohash search;
    if (!bencode::d::pair(p, "search", search.id)) {
      return false;
    }
    std::size_t sec = 0;
    if (!bencode::d::pair(p, "timeout", sec)) {
      return false;
    }

    return f(ctx, search, sp::Seconds(sec));
  });
}
} // namespace request

} // namespace d
} // namespace krpc

#endif
