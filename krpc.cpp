#include "dslbencode.h"
#include "krpc.h"
#include <cassert>
#include <cstring>
#include <tuple>
#include <type_traits>

//=KRPC==================================================================
namespace krpc {
/*krpc*/
template <typename F>
static bool
message(sp::Buffer &buf, const Transaction &t, const char *mt,
        const char *query, F f) noexcept {
  return bencode::e::dict(
      buf, //
      [&t, &mt, query, &f](sp::Buffer &b) {

        if (!f(b)) {
          return false;
        }

        if (query) {
          if (!bencode::e::pair(b, "q", query)) {
            return false;
          }
        }

        // transaction: t
        assert(t.length > 0);
        if (!bencode::e::pair(b, "t", t.id, t.length)) {
          return false;
        }

        sp::byte version[4] = {0};
        {
          version[0] = 's';
          version[1] = 'p';
          version[2] = 0;
          version[3] = 1;
        }
        if (!bencode::e::pair(b, "v", version, sizeof(version))) {
          return false;
        }

        // message_type[reply: r, query: q]
        if (!bencode::e::pair(b, "y", mt)) {
          return false;
        }

        return true;
      });
} // krpc::message()

template <typename F>
static bool
resp(sp::Buffer &buf, const Transaction &t, F f) noexcept {
  return message(buf, t, "r", nullptr, [&f](auto &b) { //
    if (!bencode::e::value(b, "r")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b2) { //
      return f(b2);
    });
  });
} // krpc::resp()

template <typename F>
static bool
req(sp::Buffer &buf, const Transaction &t, const char *query, F f) noexcept {
  return message(buf, t, "q", query, [&f](auto &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b2) { //
      return f(b2);
    });
  });
} // krpc::req()

template <typename F>
static bool
err(sp::Buffer &buf, const Transaction &t, F f) noexcept {
  const char *query = nullptr;
  return message(buf, t, "e", query, [&f](auto &b) { //
    if (!bencode::e::value(b, "e")) {
      return false;
    }

    return f(b);
  });
} // krpc::err()

namespace request {
/*krpc::request*/
bool
ping(sp::Buffer &buf, const Transaction &t, const dht::NodeId &send) noexcept {
  return req(buf, t, "ping", [&send](auto &b) { //
    if (!bencode::e::pair(b, "id", send.id, sizeof(send.id))) {
      return false;
    }

    return true;
  });
} // request::ping()

bool
find_node(sp::Buffer &buf, const Transaction &t, const dht::NodeId &self,
          const dht::NodeId &search) noexcept {
  return req(buf, t, "find_node", [self, search](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", self.id, sizeof(self.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "target", search.id, sizeof(search.id))) {
      return false;
    }
    return true;
  });
} // request::find_node()

bool
get_peers(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
          const dht::Infohash &infohash) noexcept {
  return req(buf, t, "get_peers", [id, infohash](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "info_hash", infohash.id, sizeof(infohash.id))) {
      return false;
    }
    return true;
  });
} // request::get_peers()

bool
announce_peer(sp::Buffer &buffer, const Transaction &t, const dht::NodeId &self,
              bool implied_port, const dht::Infohash &infohash, Port port,
              const dht::Token &token) noexcept {
  return req(
      buffer, t, "announce_peer",
      [&self, &implied_port, &infohash, &port, &token](sp::Buffer &buf) { //
        if (!bencode::e::pair(buf, "id", self.id, sizeof(self.id))) {
          return false;
        }

        if (!bencode::e::pair(buf, "implied_port", implied_port)) {
          return false;
        }

        if (!bencode::e::pair(buf, "info_hash", infohash.id,
                              sizeof(infohash.id))) {
          return false;
        }

        if (!bencode::e::pair(buf, "port", port)) {
          return false;
        }

        if (!bencode::e::pair(buf, "token", token.id, token.length)) {
          return false;
        }
        return true;
      });
} // request::announce_peer()

bool
dump(sp::Buffer &b, const Transaction &t) noexcept {
  return req(b, t, "sp_dump", [](sp::Buffer &) { //

    return true;
  });
} // request::dump()

bool
statistics(sp::Buffer &b, const Transaction &t) noexcept {
  return req(b, t, "sp_statistics", [](sp::Buffer &) { //

    return true;
  });
}

} // namespace request

namespace response {
/*krpc::response*/
bool
ping(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id) noexcept {
  return resp(buf, t, [&id](sp::Buffer &b) { //

    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    return true;
  });
} // response::ping()

bool
find_node(sp::Buffer &buf, const Transaction &t, //
          const dht::NodeId &id, const dht::Node **target,
          std::size_t length) noexcept {
  return resp(buf, t, [&id, &target, &length](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    return bencode::e::pair_compact(b, "target", target, length);
  });
} // response::find_node()

bool
get_peers(sp::Buffer &buf, const Transaction &t, //
          const dht::NodeId &id, const dht::Token &token,
          const dht::Node **nodes, std::size_t length) noexcept {
  return resp(buf, t, [&id, &token, &nodes, &length](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    return bencode::e::pair_compact(b, "nodes", nodes, length);
  });
} // response::get_peers()

bool
get_peers(sp::Buffer &buf,
          const Transaction &t, //
          const dht::NodeId &id, const dht::Token &token,
          const dht::Peer *values) noexcept {
  return resp(buf, t, [&id, &token, values](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    return bencode::e::pair_compact(b, "values", values);
  });
} // response::get_peers()

bool
announce_peer(sp::Buffer &buf, const Transaction &t,
              const dht::NodeId &id) noexcept {
  return resp(buf, t, [&id](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    return true;
  });
} // response::announce_peer()

bool
error(sp::Buffer &buf, const Transaction &t, Error e,
      const char *msg) noexcept {
  return err(buf, t, [e, msg](auto &b) { //

    std::tuple<Error, const char *> tt(e, msg);
    return bencode::e::list(b, &tt, [](auto &b2, void *a) {
      auto targ = *((std::tuple<Error, const char *> *)a);

      using Et = std::underlying_type<Error>::type;
      auto error = static_cast<Et>(std::get<0>(targ));

      if (!bencode::e::value(b2, error)) {
        return false;
      }

      const char *emsg = std::get<1>(targ);
      if (!bencode::e::value(b2, emsg)) {
        return false;
      }

      return true;
    });
  });
} // response::error()

bool
dump(sp::Buffer &buf, const Transaction &t, const dht::DHT &dht) noexcept {
  return resp(buf, t, [&dht](auto &b) {
    if (!bencode::e::pair(b, "id", dht.id.id, sizeof(dht.id.id))) {
      return false;
    }
    // TODO?
    // if (!bencode::e::pair(b, "ip", dht.ip)) {
    //   return false;
    // }
    if (!bencode::e::pair(b, "peer_db", dht.lookup_table)) {
      return false;
    }
    if (!bencode::e::pair(b, "routing", dht.root)) {
      return false;
    }
    if (!bencode::e::pair(b, "last_activity",
                          std::uint64_t(dht.last_activity))) {
      return false;
    }
    if (!bencode::e::pair(b, "total_nodes", dht.total_nodes)) {
      return false;
    }
    if (!bencode::e::pair(b, "bad_nodes", dht.bad_nodes)) {
      return false;
    }
    if (!bencode::e::priv::pair(b, "boostrap", dht.bootstrap_contacts)) {
      return false;
    }
    if (!bencode::e::pair(b, "active_searches", dht.active_searches)) {
      return false;
    }
    return true;
  });
} // response::dump()

bool
statistics(sp::Buffer &buf, const Transaction &t,
           const dht::Stat &stat) noexcept {
  return resp(buf, t, [&stat](auto &b) { //
    if (!bencode::e::priv::pair(b, "sent", stat.sent)) {
      return false;
    }
    if (!bencode::e::priv::pair(b, "received", stat.received)) {
      return false;
    }
    if (!bencode::e::pair(b, "known_tx", stat.known_tx)) {
      return false;
    }
    if (!bencode::e::pair(b, "unknown_tx", stat.unknown_tx)) {
      return false;
    }
    return true;
  });
}

bool
search(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

} // namespace response

} // namespace krpc
