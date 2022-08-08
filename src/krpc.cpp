#include "krpc.h"
#include "cache.h"
#include "encode_bencode.h"
#include "krpc_shared.h"

#include <cstring>
#include <list/LinkedList.h>
#include <tuple>
#include <type_traits>

//=KRPC==================================================================
namespace krpc {
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

//=====================================
bool
sample_infohashes(sp::Buffer &b, const Transaction &t, const dht::NodeId &self,
                  const dht::Key &target) noexcept {
  return req(b, t, "sample_infohashes", [&self, &target](sp::Buffer &buf) { //
    if (!bencode::e::pair(buf, "id", self.id, sizeof(self.id))) {
      return false;
    }
    if (!bencode::e::pair(buf, "target", target, sizeof(target))) {
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

    return sp::bencode::e<sp::Buffer>::pair_id_contact_compact(b, "target",
                                                               target, length);
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

    return sp::bencode::e<sp::Buffer>::pair_id_contact_compact(b, "nodes",
                                                               nodes, length);
  });
} // response::get_peers()

//=====================================
bool
get_peers_peers(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
                const dht::Token &token,
                const sp::UinArray<Contact> &values) noexcept {
  return resp(buf, t, [&id, &token, &values](auto &b) {
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    return sp::bencode::e<sp::Buffer>::pair_compact(b, "values", values);
  });
} // response::get_peers()

//=====================================
bool
get_peers_scrape(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
                 const dht::Token &token, const uint8_t seeds[256],
                 const uint8_t peers[256]) noexcept {
  return resp(buf, t, [&id, &token, &seeds, &peers](auto &b) {
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    if (!bencode::e::pair(b, "BFsd", seeds)) {
      return false;
    }

    if (!bencode::e::pair(b, "BFpe", peers)) {
      return false;
    }

    return true;
  });
}

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
sample_infohashes(
    sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
    std::uint32_t interval, const dht::Node **nodes, size_t l_nodes,
    std::uint32_t num,
    const sp::UinStaticArray<dht::Infohash, 20> &samples) noexcept {
  return resp(buf, t,
              [&id, &interval, &nodes, l_nodes, &num, &samples](auto &b) { //
                if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
                  return false;
                }

                if (!bencode::e::pair(b, "interval", interval)) {
                  return false;
                }

                if (!sp::bencode::e<sp::Buffer>::pair_id_contact_compact(
                        b, "target", nodes, l_nodes)) {
                  return false;
                }

                if (!bencode::e::pair(b, "num", num)) {
                  return false;
                }

                if (!sp::bencode::e<sp::Buffer>::pair_compact(
                        b, "samples", samples.data(), samples.length)) {
                  return false;
                }

                return true;
              });
} // response::sample_infohashes()

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

} // namespace response
} // namespace krpc
