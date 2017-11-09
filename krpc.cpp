#include "BEncode.h"
#include "krpc.h"
#include <cstring>

namespace krpc {
template <typename F>
static bool
message(sp::Buffer &buf, const Transaction &t, const char *mt, const char *q,
        F f) noexcept {
  return bencode::e::dict(buf, //
                          [&t, &mt, q, &f](sp::Buffer &b) {
                            if (!bencode::e::pair(b, "t", t.id)) {
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

template <typename F>
static bool
resp(sp::Buffer &buf, const Transaction &t, const char *q, F f) noexcept {
  return message(buf, t, "r", q, [&f](auto &b) { //
    if (!bencode::e::value(b, "r")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b) { //
      return f(b);
    });
  });
}

template <typename F>
static bool
req(sp::Buffer &buf, const Transaction &t, const char *q, F f) noexcept {
  return message(buf, t, "q", q, [&f](auto &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b) { //
      return f(b);
    });
  });
}

namespace request {
bool
ping(sp::Buffer &buf, const Transaction &t, const dht::NodeId &send) noexcept {
  return req(buf, t, "ping", [&send](auto &b) { //
    if (!bencode::e::pair(b, "id", send.id)) {
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

    if (!bencode::e::pair(b, "target", search.id)) {
      return false;
    }
    return true;
  });
} // request::find_node()

bool
get_peers(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
          const dht::Infohash &infohash) noexcept {
  return req(buf, t, "get_peers", [id, infohash](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", id.id)) {
      return false;
    }

    if (!bencode::e::pair(b, "info_hash", infohash.id)) {
      return false;
    }
    return true;
  });
} // request::get_peers()

bool
announce_peer(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id,
              bool implied_port, const dht::Infohash &infohash,
              std::uint16_t port, const char *token) noexcept {
  return req(
      buf, t, "announce_peer",
      [&id, &implied_port, &infohash, &port, &token](sp::Buffer &buf) { //
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
} // request::announce_peer()

} // namespace request

namespace response {
bool
ping(sp::Buffer &buf, const Transaction &t, const dht::NodeId &id) noexcept {
  return resp(buf, t, "ping", [&id](sp::Buffer &b) { //

    if (!bencode::e::pair(b, "id", id.id)) {
      return false;
    }
    return true;
  });
} // response::ping()

template <typename T, typename F>
static bool
for_all(const sp::list<T> *l, F f) {
  while (l != nullptr && l->size > 0) {
    if (!f(l)) {
      return false;
    }
    l = l->next;
  }
  return true;
}

template <typename T>
static bool
encode_list(sp::Buffer &buf, const char *key,
            const sp::list<T> *list) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  void *arg = (void *)list;
  return bencode::e::list(buf, arg, [](auto &b, void *a) {

    const sp::list<T> *l = (const sp::list<T> *)a;
    return for_all(l, [&b](const sp::list<T> *l) {

      if (!bencode::e::value(b, l->value)) {
        return false;
      }
      return true;
    });
  });
}

bool
find_node(sp::Buffer &buf, const Transaction &t, //
          const dht::NodeId &id, const sp::list<dht::NodeId> *target) noexcept {
  return resp(buf, t, "find_node", [&id, target](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id)) {
      return false;
    }

    return encode_list<dht::NodeId>(b, "target", target);
  });
} // response::find_node()

bool
get_peers(sp::Buffer &buf,
          const Transaction &t, //
          const dht::NodeId &id, const dht::Token &token,
          const sp::list<dht::Node> *values) noexcept {
  return resp(buf, t, "get_peers", [&id, &token, &values](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id)) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id)) {
      return false;
    }

    return encode_list<dht::Node>(b, "values", values);
  });
} // response::get_peers()

bool
get_peers(sp::Buffer &buf, const Transaction &t, //
          const dht::NodeId &id, const dht::Token &token,
          const sp::list<dht::NodeId> *nodes) noexcept {
  return resp(buf, t, "get_peers", [&id, &token, &nodes](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id)) {
      return false;
    }

    if (!bencode::e::pair(b, "token", token.id)) {
      return false;
    }

    return encode_list<dht::NodeId>(b, "nodes", nodes);
  });
} // response::get_peers()

bool
announce_peer(sp::Buffer &buf, const Transaction &t,
              const dht::NodeId &id) noexcept {
  return resp(buf, t, "announce_peer", [&id](auto &b) { //

    if (!bencode::e::pair(b, "id", id.id)) {
      return false;
    }

    return true;
  });
} // response::announce_peer()

} // namespace response

} // namespace krpc
