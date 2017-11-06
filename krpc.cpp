#include "BEncode.h"
#include "krpc.h"
#include <string.h>

namespace krpc {

template <std::size_t SIZE>
static bool
transaction(char (&t)[SIZE]) noexcept {
  strcpy(t, "aa");
  return true;
}

template <typename F>
static bool
message(sp::Buffer &buf, const char *mt, const char *q, F f) noexcept {
  // Transaction id
  char t[3];
  transaction(t);

  return bencode::e::dict(buf, //
                          [&](sp::Buffer &b) {
                            if (!bencode::e::pair(b, "t", t)) {
                              return false;
                            }
                            if (!bencode::e::pair(b, "y", mt)) {
                              return false;
                            }
                            // Client identifier
                            // if (!encodePair(b, "v", "SP")) {
                            //   return false;
                            // }
                            if (!bencode::e::pair(b, "q", q)) {
                              return false;
                            }
                            if (!f(b)) {
                              return false;
                            }
                            return true;
                          });
}

namespace request {
bool
ping(sp::Buffer &buf, const dht::NodeId &sender) noexcept {
  return message(buf, "q", "ping", [&sender](sp::Buffer &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [&sender](sp::Buffer &b) {
      if (!bencode::e::pair(b, "id", sender.id, sizeof(sender.id))) {
        return false;
      }
      return true;
    });
  });
} // request::ping()

bool
find_node(sp::Buffer &buf, const dht::NodeId &self,
          const dht::NodeId &search) noexcept {
  using namespace bencode;
  return message(buf, "q", "find_node", [self, search](sp::Buffer &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(
        b,                              //
        [self, search](sp::Buffer &b) { //
          if (!bencode::e::pair(b, "id", self.id, sizeof(self.id))) {
            return false;
          }

          if (!bencode::e::pair(b, "target", search.id, sizeof(search.id))) {
            return false;
          }
          return true;
        });
  });
} // request::find_node()

bool
get_peers(sp::Buffer &buf, const dht::NodeId &id,
          const dht::Infohash &infohash) noexcept {

  using namespace bencode;
  return message(buf, "q", "get_peers", [id, infohash](sp::Buffer &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [id, infohash](sp::Buffer &b) { //
      if (!bencode::e::pair(b, "id", id.id)) {
        return false;
      }

      if (!bencode::e::pair(b, "info_hash", infohash.id)) {
        return false;
      }
      return true;
    });
  });
} // request::get_peers()

bool
announce_peer(sp::Buffer &buf, const dht::NodeId &id, bool implied_port,
              const dht::Infohash &infohash, std::uint16_t port,
              const char *token) noexcept {

  using namespace bencode;
  return message(
      buf, "q", "announce_peer",
      [id, implied_port, infohash, port, token](sp::Buffer &b) { //
        if (!bencode::e::value(b, "a")) {
          return false;
        }

        return bencode::e::dict(
            b,
            [&](sp::Buffer &buf) { //
              if (!bencode::e::pair(buf, "id", id.id)) {
                return false;
              }

              if (!bencode::e::pair(buf, "implied_port", implied_port)) {
                return false;
              }

              if (!bencode::e::pair(buf, "info_hash", infohash.id)) {
                return false;
              }

              if (!bencode::e::pair(buf, "port", port)) {
                return false;
              }

              if (!bencode::e::pair(buf, "token", token)) {
                return false;
              }
              return true;
            });
      });
} // request::announce_peer()

} // namespace request

namespace response {
bool
ping(sp::Buffer &buf, const dht::NodeId &id) noexcept {
  using namespace bencode;
  return message(buf, "r", "ping", [id](sp::Buffer &b) { //
    return bencode::e::dict(b,
                            [id](sp::Buffer &b) { //
                              if (!bencode::e::value(b, "r")) {
                                return false;
                              }

                              if (!bencode::e::pair(b, "id", id.id)) {
                                return false;
                              }
                              return true;
                            });
  });
} // response::ping()

bool
find_node(sp::Buffer &buf, const dht::NodeId &id, const char *target) noexcept {
  using namespace bencode;
  return message(buf, "r", "find_node", [id, target](sp::Buffer &b) { //
    return bencode::e::dict(
        b,                            //
        [id, target](sp::Buffer &b) { //
          if (!bencode::e::value(b, "r")) {
            return false;
          }

          if (!bencode::e::pair(b, "id", id.id, sizeof(id.id))) {
            return false;
          }

          if (!bencode::e::pair(b, "target", target)) {
            return false;
          }
          return true;
        });
  });
} // response::find_node()

bool
announce_peer(sp::Buffer &buf, const dht::NodeId &id) noexcept {
  using namespace bencode;
  return message(buf, "r", "announce_peerid", [id](sp::Buffer &b) { //
    return bencode::e::dict(b,
                            [id](sp::Buffer &b) { //
                              if (!bencode::e::value(b, "r")) {
                                return false;
                              }

                              if (!bencode::e::pair(b, "id", id.id)) {
                                return false;
                              }
                              return true;
                            });
  });
} // response::announce_peer()

} // namespace response

} // namespace krpc
