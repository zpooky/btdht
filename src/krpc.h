#ifndef SP_MAINLINE_DHT_KRPC_H
#define SP_MAINLINE_DHT_KRPC_H

#include "Log.h"
#include "bencode_offset.h"
#include "bencode_print.h"
#include "shared.h"
#include <cstring>
#include <util/assert.h>

namespace krpc {
enum class MessageType : char { query = 'q', response = 'r', error = 'e' };
enum class Query { ping, find_node, get_peers, announce_peer };
enum class Error {
  generic_error = 201,
  server_error = 202,
  protocol_error = 203,
  method_unknown = 204
};

//=====================================
namespace request {
/*krpc::request*/
/* DHT interface */
//{
bool
ping(sp::Buffer &, const Transaction &, const dht::NodeId &sender) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, const dht::NodeId &self,
          const dht::NodeId &search) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, const dht::NodeId &,
          const dht::Infohash &) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, const dht::NodeId &self,
              bool implied_port, const dht::Infohash &, Port port,
              const dht::Token &) noexcept;
//}

//=====================================
/*private interface*/
//{
bool
dump(sp::Buffer &b, const Transaction &) noexcept;

bool
statistics(sp::Buffer &b, const Transaction &t) noexcept;

bool
search(sp::Buffer &b, const Transaction &, const dht::Infohash &,
       std::size_t) noexcept;

bool
stop_search(sp::Buffer &b, const Transaction &, const dht::Infohash &) noexcept;
//}
} // namespace request

//=====================================
namespace response {
/*krpc::response*/
/* DHT interface */
//{
bool
ping(sp::Buffer &, const Transaction &, const dht::NodeId &receiver) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, const dht::NodeId &,
          const dht::Node **target, std::size_t) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, const dht::NodeId &id,
          const dht::Token &, const dht::Node **, std::size_t) noexcept;

bool
get_peers_peers(sp::Buffer &, const Transaction &, const dht::NodeId &id,
                const dht::Token &, const sp::UinArray<Contact> &) noexcept;

bool
get_peers_scrape(sp::Buffer &, const Transaction &, const dht::NodeId &id,
                 const dht::Token &, const uint8_t seeds[256],
                 const uint8_t peers[256]) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, const dht::NodeId &) noexcept;

bool
error(sp::Buffer &, const Transaction &, Error, const char *) noexcept;
//}

//=====================================
/*private interface*/
//{
bool
dump(sp::Buffer &b, const Transaction &t, const dht::DHT &) noexcept;

bool
statistics(sp::Buffer &b, const Transaction &t, const dht::Stat &) noexcept;

bool
search(sp::Buffer &b, const Transaction &t) noexcept;

bool
search_stop(sp::Buffer &b, const Transaction &t) noexcept;

bool
announce_this(sp::Buffer &b, const Transaction &t) noexcept;
//}

} // namespace response

//=====================================
namespace priv {
namespace event {
template <typename Contacts>
bool
found(sp::Buffer &, const dht::Infohash &, const Contacts &) noexcept;

} // namespace event
} // namespace priv

//=====================================
namespace d {
template <typename F>
bool
krpc(ParseContext &pctx, F handle) {
  return bencode::d::dict(pctx.decoder, [&](auto &p) {
    bool do_mark_dict = false;
    bool t = false;
    bool y = false;
    bool q = false;
    bool ip_handled = false;
    bool v = false;

    ssize_t mark = -1;
    std::size_t mark_end = 0;

    char wkey[128] = {0};
    sp::byte wvalue[128] = {0};
  Lstart:
    do_mark_dict = false;

    // TODO length compare for all raw indexing everywhere!!
    if (p.raw[p.pos] != 'e') {
      const std::size_t before = p.pos;
      krpc::Transaction &tx = pctx.tx;
      if (!t && bencode::d::pair(p, "t", tx.id, tx.length)) {
        t = true;
        goto Lstart;
      } else {
        assertx(before == p.pos);
      }

      if (!y && bencode::d::pair(p, "y", pctx.msg_type)) {
        y = true;
        goto Lstart;
      } else {
        assertx(before == p.pos);
      }

      if (!q && bencode::d::pair(p, "q", pctx.query)) {
        q = true;
        goto Lstart;
      } else {
        assertx(before == p.pos);
      }

      {
        Contact ip;
        if (!ip_handled && bencode::d::pair(p, "ip", ip)) {
          pctx.ip_vote = ip;
          assertx(bool(pctx.ip_vote));
          ip_handled = true;
          goto Lstart;
        } else {
          assertx(before == p.pos);
        }
      }

      if (!v && bencode::d::pair(p, "v", pctx.remote_version)) {
        v = true;
        goto Lstart;
      } else {
        assertx(before == p.pos);
      }

      if (bencode::d::pair(p, "ro", pctx.ro)) {
        goto Lstart;
      } else {
        assertx(before == p.pos);
      }

      // the application layer dict[request argument:a, reply: r]
      if (bencode::d::peek(p, "a")) {
        bencode::d::value(p, "a");
        mark = p.pos;
        do_mark_dict = true;
      } else if (bencode::d::peek(p, "r")) {
        bencode::d::value(p, "r");
        mark = p.pos;
        do_mark_dict = true;
      } else if (bencode::d::peek(p, "e")) {
        bencode::d::value(p, "e");
        mark = p.pos;

        if (!bencode::d::list_wildcard(p)) {
          logger::receive::parse::error(pctx.ctx, p, "Invalid krpc");
          return false;
        }
        mark_end = p.pos;

        goto Lstart;
      }

      if (do_mark_dict) {
        // since filds can be located in random order we store the the location
        // of the 'a'/'r' dict for later processing
        if (!bencode::d::dict_wildcard(p)) {
          logger::receive::parse::error(pctx.ctx, p, "Invalid krpc");
          return false;
        }
        mark_end = p.pos;

        goto Lstart;
      } else {
        // parse and ignore unknown attributes for future compatability
        if (!bencode::d::pair_any(p, wkey, wvalue)) {
          logger::receive::parse::error(pctx.ctx, p, "Invalid krpc 2");
          return false;
        } else {
          fprintf(stderr, "krpc any[%s, %s]\n", wkey, wvalue);
        }
        goto Lstart;
      }
    }

    auto is_query = [&]() { //
      return std::strcmp("q", pctx.msg_type) == 0;
    };
    auto is_reply = [&] { //
      return std::strcmp("r", pctx.msg_type) == 0;
    };
    auto is_error = [&] { //
      return std::strcmp("e", pctx.msg_type) == 0;
    };

    /* Verify that required fields are present */
    if (is_error()) {
      if (!(t)) {
        logger::receive::parse::error(pctx.ctx, p, "'error' missing 't'");
        return false;
      }
    } else if (is_query()) {
      if (!(t && y && q)) {
        logger::receive::parse::error(pctx.ctx, p,
                                      "'query' missing 't' or 'y' or 'q'");
        return false;
      }
    } else if (is_reply()) {
      if (!(t && y)) {
        logger::receive::parse::error(pctx.ctx, p,
                                      "'reply' missing 't' or 'y'");
        return false;
      }
    } else {
      char msg[128];
      sprintf(msg, "Unknown message type '%s'", pctx.msg_type);
      logger::receive::parse::error(pctx.ctx, p, msg);
      return false;
    }

    if (mark < 0) {
      logger::receive::parse::error(pctx.ctx, p, "missing 'r'/'a' dict body");
      return false;
    }

    /* TODO document this */
    sp::Buffer copy(p, mark, mark_end);
    krpc::ParseContext abbriged(pctx, copy);
    return handle(abbriged);
  });
}

namespace request {
//=====================================
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

//=====================================
template <typename Ctx, typename F>
bool
search_stop(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) {
    dht::Infohash search;
    if (!bencode::d::pair(p, "search", search.id)) {
      return false;
    }

    return f(ctx, search);
  });
}

//=====================================
template <typename Ctx, typename F>
bool
announce_this(Ctx &ctx, F f) noexcept {
  return bencode::d::dict(ctx.in, [&ctx, f](auto &p) {
    dht::DHT &dht = ctx.dht;
    auto &contacts = dht.recycle_contact_list;

    dht::Infohash id;
    if (!bencode::d::pair(p, "id", id.id)) {
      return false;
    }

    clear(contacts);

    if (!bencode::d::peers(p, "contacts", contacts)) {
      return false;
    }

    return f(ctx, id, contacts);
  });
}

//=====================================
} // namespace request
} // namespace d
} // namespace krpc

#endif
