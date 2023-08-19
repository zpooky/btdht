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
bool
request::ping(sp::Buffer &buf, const Transaction &t,
              const dht::NodeId &send) noexcept {
  return req(buf, t, "ping", [&send](auto &b) { //
    if (!bencode::e::pair(b, "id", send.id, sizeof(send.id))) {
      return false;
    }

    return true;
  });
}

bool
request::find_node(sp::Buffer &buf, const Transaction &t,
                   const dht::NodeId &self, const dht::NodeId &search, bool n4,
                   bool n6) noexcept {
  return req(buf, t, "find_node", [self, search, n4, n6](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", self.id, sizeof(self.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "target", search.id, sizeof(search.id))) {
      return false;
    }
    sp::UinStaticArray<std::string, 2> want;
    if (n4) {
      insert(want, "n4");
    }

    if (n6) {
      insert(want, "n6");
    }

    if (!sp::bencode::e<sp::Buffer>::pair(b, "want", want)) {
      return false;
    }

    return true;
  });
}

bool
request::get_peers(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
                   const dht::Infohash &infohash, bool n4, bool n6) noexcept {
  return req(buf, t, "get_peers", [id, infohash, n4, n6](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "info_hash", infohash.id, sizeof(infohash.id))) {
      return false;
    }
    sp::UinStaticArray<std::string, 2> want;
    if (n4) {
      insert(want, "n4");
    }

    if (n6) {
      insert(want, "n6");
    }

    if (!sp::bencode::e<sp::Buffer>::pair(b, "want", want)) {
      return false;
    }
    return true;
  });
}

bool
request::get_peers_scrape(sp::Buffer &buf, const Transaction &t,
                          const dht::NodeId &id, const dht::Infohash &infohash,
                          bool n4, bool n6, bool scrape) noexcept {
  return req(buf, t, "get_peers",
             [id, infohash, n4, n6, scrape](sp::Buffer &b) { //
               if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
                 return false;
               }

               if (!bencode::e::pair(b, "info_hash", infohash.id,
                                     sizeof(infohash.id))) {
                 return false;
               }
               sp::UinStaticArray<std::string, 2> want;
               if (n4) {
                 insert(want, "n4");
               }

               if (n6) {
                 insert(want, "n6");
               }

               if (!sp::bencode::e<sp::Buffer>::pair(b, "want", want)) {
                 return false;
               }

               if (!sp::bencode::e<sp::Buffer>::pair(b, "scrape", scrape)) {
                 return false;
               }
               return true;
             });
}

bool
request::announce_peer(sp::Buffer &buffer, const Transaction &t,
                       const dht::NodeId &self, bool implied_port,
                       const dht::Infohash &infohash, Port port,
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
}

//=====================================
bool
request::sample_infohashes(sp::Buffer &b, const Transaction &t,
                           const dht::NodeId &self,
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

//=====================================
bool
response::ping(sp::Buffer &buf, const Transaction &t,
               const dht::NodeId &id) noexcept {
  return resp(buf, t, [&id](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    return true;
  });
}

//=====================================
bool
response::find_node(sp::Buffer &buf, const Transaction &t, //
                    const dht::NodeId &id, bool n4, const dht::Node **nodes4,
                    std::size_t length4, bool n6) noexcept {
  return resp(buf, t, [&id, n4, &nodes4, &length4, n6](auto &b) { //
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (n4) {
      if (!sp::bencode::e<sp::Buffer>::pair_id_contact_compact(
              b, "nodes", nodes4, length4)) {
        return false;
      }
    }
    if (n6) {
      if (!sp::bencode::e<sp::Buffer>::pair_id_contact_compact(b, "nodes6",
                                                               nullptr, 0)) {
        return false;
      }
    }
    return true;
  });
}

//=====================================
bool
response::get_peers(sp::Buffer &buf, const Transaction &t, //
                    const dht::NodeId &id, const dht::Token &token, bool n4,
                    const dht::Node **nodes4, std::size_t length4,
                    bool n6) noexcept {
  return resp(buf, t, [&id, &token, n4, &nodes4, &length4, n6](auto &b) { //
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    if (n4) {
      if (!sp::bencode::e<sp::Buffer>::pair_id_contact_compact(
              b, "nodes", nodes4, length4)) {
        return false;
      }
    }
    if (n6) {
      if (!sp::bencode::e<sp::Buffer>::pair_id_contact_compact(b, "nodes6",
                                                               nullptr, 0)) {
        return false;
      }
    }

    return true;
  });
}

//=====================================
bool
response::get_peers_peers(sp::Buffer &buf, const Transaction &t,
                          const dht::NodeId &id, const dht::Token &token,
                          const sp::UinArray<Contact> &values) noexcept {
  return resp(buf, t, [&id, &token, &values](auto &b) {
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      assertx(false);
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      assertx(false);
      return false;
    }

    return sp::bencode::e<sp::Buffer>::pair_string_list_contact(b, "values",
                                                                values);
  });
}

//=====================================
bool
response::get_peers_scrape(sp::Buffer &buf, const Transaction &t,
                           const dht::NodeId &id, const dht::Token &token,
                           const uint8_t seeds[256],
                           const uint8_t peers[256]) noexcept {
  return resp(buf, t, [&id, &token, &seeds, &peers](auto &b) {
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id, token.length)) {
      return false;
    }

    const sp::byte *BFsd = seeds;
    if (!bencode::e::pair(b, "BFsd", BFsd, 256)) {
      return false;
    }

    const sp::byte *BFpe = peers;
    if (!bencode::e::pair(b, "BFpe", BFpe, 256)) {
      return false;
    }

    return true;
  });
}

//=====================================
bool
response::announce_peer(sp::Buffer &buf, const Transaction &t,
                        const dht::NodeId &id) noexcept {
  return resp(buf, t, [&id](auto &b) { //
    if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
      return false;
    }

    return true;
  });
}

//=====================================
bool
response::sample_infohashes(
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
}

//=====================================
bool
response::error(sp::Buffer &buf, const Transaction &t, Error e,
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
}

} // namespace krpc
