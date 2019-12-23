#include "krpc.h"
#include "cache.h"
#include "dslbencode.h"
#include <cstring>
#include <list/LinkedList.h>
#include <tree/bst_extra.h>
#include <tuple>
#include <type_traits>

//=KRPC==================================================================
namespace krpc {
/*krpc*/
template <typename F>
static bool
message(sp::Buffer &buf, const Transaction &t, const char *mt,
        const char *query, F f) noexcept {
  auto cb = [&t, &mt, query, &f](sp::Buffer &b) {
    if (!f(b)) {
      return false;
    }

    if (query) {
      if (!bencode::e::pair(b, "q", query)) {
        return false;
      }
    }

    // transaction: t
    assertx(t.length > 0);
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
  };

  return bencode::e::dict(buf, cb);
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

//=====================================
bool
statistics(sp::Buffer &b, const Transaction &t) noexcept {
  return req(b, t, "sp_statistics", [](sp::Buffer &) { //
    return true;
  });
}

//=====================================
bool
search(sp::Buffer &b, const Transaction &t, const dht::Infohash &search,
       std::size_t timeout) noexcept {
  return req(b, t, "sp_search", [&search, &timeout](sp::Buffer &buffer) {
    /**/
    if (!bencode::e::pair(buffer, "search", search.id)) {
      return false;
    }
    if (!bencode::e::pair(buffer, "timeout", timeout)) {
      return false;
    }
    return true;
  });
}

bool
stop_search(sp::Buffer &b, const Transaction &t,
            const dht::Infohash &search) noexcept {
  return req(b, t, "sp_search_stop", [&search](sp::Buffer &buffer) {
    /**/
    if (!bencode::e::pair(buffer, "search", search.id)) {
      return false;
    }
    return true;
  });
}

} // namespace request

namespace response {
//=====================================
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

//=====================================
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

//=====================================
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

//=====================================
bool
get_peers(sp::Buffer &buf,
          const Transaction &t, //
          const dht::NodeId &id, const dht::Token &token,
          const sp::UinArray<dht::Peer> &values) noexcept {
  return resp(buf, t, [&id, &token, &values](auto &b) {
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    return bencode::e::pair_compact(b, "values", values);
  });
} // response::get_peers()

//=====================================
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

//=====================================
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

//=====================================
bool
dump(sp::Buffer &buf, const Transaction &t, const dht::DHT &dht) noexcept {
  return resp(buf, t, [&dht](auto &b) {
    if (!bencode::e::pair(b, "id", dht.id.id, sizeof(dht.id.id))) {
      return false;
    }

    {
      if (!bencode::e::value(b, "cache")) {
        return false;
      }

      bool res = bencode::e::dict(b, [&dht](auto &b2) {
        if (!bencode::e::pair(b2, "min_read_idx",
                              sp::cache_read_min_idx(dht))) {
          return false;
        }

        if (!bencode::e::pair(b2, "max_read_idx",
                              sp::cache_read_max_idx(dht))) {
          return false;
        }

        if (!bencode::e::pair(b2, "contacts", sp::cache_contacts(dht))) {
          return false;
        }

        if (!bencode::e::pair(b2, "write_idx", sp::cache_write_idx(dht))) {
          return false;
        }

        return true;
      });
      if (!res) {
        return false;
      }
    }

    {
      if (!bencode::e::value(b, "db")) {
        return false;
      }

      bool res = bencode::e::dict(b, [&dht](auto &b2) {
        binary::rec::inorder(dht.lookup_table, [&b2](dht::KeyValue &e) -> bool {
          char buffer[64]{0};
          assertx_n(to_string(e.id, buffer));
          std::uint64_t l(sp::n::length(e.peers));

          return bencode::e::pair(b2, buffer, l);
        });
        return true;
      });

      if (!res) {
        return false;
      }
    }

    {
      if (!bencode::e::value(b, "ip_election")) {
        return false;
      }

      bool res = bencode::e::dict(b, [&dht](auto &b2) {
        return for_all(dht.election.table, [&b2](const auto &e) {
          Contact c = std::get<0>(e);
          std::size_t votes = std::get<1>(e);
          char str[64] = {0};
          if (!to_string(c, str)) {
            assertx(false);
          }

          return bencode::e::pair(b2, str, votes);
        });
      });

      if (!res) {
        return false;
      }
    }

    if (!bencode::e::pair(b, "root", dht.root)) {
      return false;
    }
    std::uint64_t la(dht.last_activity);
    if (!bencode::e::pair(b, "last_activity", la)) {
      return false;
    }
    if (!bencode::e::pair(b, "total_nodes", dht.total_nodes)) {
      return false;
    }
    if (!bencode::e::pair(b, "bad_nodes", dht.bad_nodes)) {
      return false;
    }
    // if (!bencode::e::priv::pair(b, "boostrap", dht.bootstrap)) {
    //   return false;
    // }
    if (!bencode::e::pair(b, "active_find_nodes", dht.active_find_nodes)) {
      return false;
    }
    return true;
  });
} // response::dump()

//=====================================
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

//=====================================
bool
search(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

bool
search_stop(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

bool
announce_this(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

} // namespace response

namespace priv {
//=====================================
namespace event {
template <typename F>
static bool
event(sp::Buffer &buf, F f) noexcept {
  Transaction t;
  t.length = 4;
  std::memcpy(t.id, "12ab", t.length);

  // return message(buf, t, "q", "found", [&f](auto &b) { //
  //   if (!bencode::e::value(b, "e")) {
  //     return false;
  //   }
  //
  //   return bencode::e::dict(b, [&f](auto &b2) { //
  //     return f(b2);
  //   });
  // });

  return message(buf, t, "q", "sp_found", [&f](auto &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b2) { //
      return f(b2);
    });
  });
} // krpc::resp()

template <typename Contacts>
bool
found(sp::Buffer &buf, const dht::Infohash &search,
      const Contacts &contacts) noexcept {
  assertx(!is_empty(contacts));

  return event(buf, [&contacts, &search](auto &b) {
    if (!bencode::e::pair(b, "id", search.id, sizeof(search.id))) {
      return false;
    }

    if (!bencode::e::value(b, "contacts")) {
      return false;
    }

    auto cb = [](sp::Buffer &b2, void *a) {
      Contacts *cx = (Contacts *)a;

      return for_all(*cx, [&](auto &c) {
        //
        return bencode::e::priv::value(b2, c);
      });
    };

    return bencode::e::list(b, (void *)&contacts, cb);
  });
}

template bool
found<sp::LinkedList<Contact>>(sp::Buffer &, const dht::Infohash &,
                               const sp::LinkedList<Contact> &) noexcept;
} // namespace event
//=====================================
} // namespace priv

} // namespace krpc
