#include "krpc.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <tuple>
#include <type_traits>

//=BEncode==================================================================
namespace bencode {
/*bencode*/
namespace e {
/*bencode::e*/

static bool
serialize(sp::Buffer &b, const Contact &p) noexcept {
  // TODO ipv4
  assert(p.ip.type == IpType::IPV4);
  const std::size_t pos = b.pos;

  Ipv4 ip = htonl(p.ip.ipv4);
  if (sp::remaining_write(b) < sizeof(ip)) {
    b.pos = pos;
    return false;
  }

  std::memcpy(b.raw + b.pos, &ip, sizeof(ip));
  b.pos += sizeof(ip);

  Port port = htons(p.port);
  if (sp::remaining_write(b) < sizeof(port)) {
    b.pos = pos;
    return false;
  }
  std::memcpy(b.raw + b.pos, &port, sizeof(port));
  b.pos += sizeof(port);

  return true;
} // bencode::e::serialize()

static bool
value(sp::Buffer &b, const Contact &p) noexcept {
  return serialize(b, p);
}

static bool
pair(sp::Buffer &buf, const char *key, const Contact &p) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return bencode::e::value(buf, p);
}

static bool
serialize(sp::Buffer &b, const dht::Node &node) noexcept {
  const std::size_t pos = b.pos;

  if (sp::remaining_read(b) < sizeof(node.id.id)) {
    b.pos = pos;
    return false;
  }
  std::memcpy(b.raw + b.pos, node.id.id, sizeof(node.id.id));
  b.pos += sizeof(node.id.id);

  if (!serialize(b, node.contact)) {
    b.pos = pos;
    return false;
  }

  return true;
} // bencode::e::serialize()

static std::size_t
size(const Contact &p) noexcept {
  // TODO ipv4
  assert(p.ip.type == IpType::IPV4);
  return sizeof(p.ip.ipv4) + sizeof(p.port);
}

static std::size_t
size(const dht::Peer &p) noexcept {
  return size(p.contact);
}

static std::size_t
size(const dht::Node &p) noexcept {
  return sizeof(p.id.id) + size(p.contact);
}

static std::size_t
size(const dht::Peer *list) noexcept {
  std::size_t result = 0;
  dht::for_all(list, [&result](const dht::Peer &ls) {
    result += size(ls);
    return true;
  });
  return result;
}

template <typename F>
static bool
for_all(const dht::Node **list, std::size_t length, F f) noexcept {
  for (std::size_t i = 0; i < length; ++i) {
    if (list[i]) {
      if (!f(*list[i])) {
        return false;
      }
    }
  }
  return true;
}

static std::size_t
size(const dht::Node **list, std::size_t length) noexcept {
  std::size_t result = 0;
  for_all(list, length, [&result](const auto &value) { //
    result += size(value);
    return true;
  });
  return result;
}

template <typename T>
static std::size_t
size(const sp::list<T> &list) noexcept {
  std::size_t result = 0;
  sp::for_each(list, [&result](const T &ls) { //
    result += size(ls);
  });
  return result;
}

bool
pair_compact(sp::Buffer &buf, const char *key, const dht::Peer *list) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  std::size_t len = size(list);
  return bencode::e::value(buf, len, (void *)list,
                           [](sp::Buffer &b, void *arg) {
                             const dht::Peer *l = (dht::Peer *)arg;

                             return dht::for_all(l, [&b](const auto &ls) {

                               if (!serialize(b, ls.contact)) {
                                 return false;
                               }

                               return true;
                             });
                           });
} // bencode::e::pair_compact()

template <typename T>
static bool
sp_list(sp::Buffer &buf, const sp::list<T> &list) noexcept {
  std::size_t len = size(list);
  return bencode::e::value(buf, len, (void *)&list, [](sp::Buffer &b, void *a) {

    const sp::list<T> *l = (sp::list<T> *)a;
    assert(l);
    return for_all(*l, [&b](const auto &value) {

      if (!serialize(b, value)) {
        return false;
      }
      return true;
    });
  });
}

template <typename T>
static bool
internal_pair(sp::Buffer &buf, const char *key,
              const sp::list<T> &list) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return sp_list(buf, list);
} // bencode::e::internal_pair()

bool
pair_compact(sp::Buffer &buf, const char *key,
             const sp::list<dht::Node> &list) noexcept {
  return internal_pair(buf, key, list);
} // bencode::e::pair()

static bool
xxx(sp::Buffer &buf, const char *key, const dht::Node **list,
    std::size_t sz) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  std::size_t len = size(list, sz);
  std::tuple<const dht::Node **, std::size_t> arg(list, sz);
  return bencode::e::value(buf, len, &arg, [](auto &b, void *a) {
    auto targ = (std::tuple<const dht::Node **, std::size_t> *)a;

    // const dht::Node **l = (dht::Node *)a;
    return for_all(std::get<0>(*targ), std::get<1>(*targ),
                   [&b](const auto &value) {

                     if (!serialize(b, value)) {
                       return false;
                     }
                     return true;
                   });
  });
} // bencode::e::xxx()

static bool
pair_compact(sp::Buffer &buf, const char *key, const dht::Node **list,
             std::size_t size) noexcept {
  return xxx(buf, key, list, size);
} // bencode::e::pair_compact()

static bool
value(sp::Buffer &buf, const dht::Node &node) noexcept {
  return bencode::e::dict(buf, [&node](sp::Buffer &b) { //

    if (!pair(b, "id", node.id.id, sizeof(node.id.id))) {
      return false;
    }

    if (!pair(b, "good", node.good)) {
      return false;
    }

    if (!pair(b, "ping_outstanding", node.ping_outstanding)) {
      return false;
    }

    return true;
  });
}

static bool
value(sp::Buffer &buf, const dht::Bucket &t) noexcept {
  return list(buf, (void *)&t, [](sp::Buffer &b, void *arg) { //
    auto *a = (dht::Bucket *)arg;

    return for_all(*a, [&b](const dht::Node &node) { //
      return value(b, node);
    });
  });
}

static bool
value(sp::Buffer &buf, const dht::RoutingTable &t) noexcept {
  // used by dump

  return dict(buf, [&t](sp::Buffer &b) {
    // dht::Infohash id; // TODO
    // if (!pair(b, "id", id.id, sizeof(id.id))) {
    //   return false;
    // }

    if (!value(b, "bucket")) {
      return false;
    }

    if (!value(b, t.bucket)) {
      return false;
    }

    return true;
  });
} // bencode::e::value()

static bool
pair(sp::Buffer &buf, const char *key, const dht::RoutingTable *t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return bencode::e::list(buf, (void *)&t, [](sp::Buffer &b, void *arg) { //
    const auto *a = (const dht::RoutingTable *)arg;

    return dht::for_all(a, [&b](const dht::RoutingTable &p) { //
      return value(b, p);
    });
  });
} // bencode::e::pair()

static bool
value(sp::Buffer &buf, const dht::Peer &t) noexcept {
  // used by dump

  return bencode::e::dict(buf, [&t](sp::Buffer &b) { //
    if (!value(b, "contact")) {
      return false;
    }
    if (!value(b, t.contact)) {
      return false;
    }

    if (!pair(b, "activity", t.activity)) {
      return false;
    }

    return true;
  });
} // bencode::e::value()

static bool
value(sp::Buffer &buf, const dht::KeyValue &t) noexcept {
  // used by dump
  return bencode::e::dict(buf, [&t](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", t.id.id, sizeof(t.id.id))) {
      return false;
    }

    if (!bencode::e::value(b, "list")) {
      return false;
    }

    return bencode::e::list(b, (void *)&t, [](sp::Buffer &b2, void *arg) { //
      const auto *a = (const dht::KeyValue *)arg;

      return for_all(a->peers, [&b2](const dht::Peer &p) { //
        return value(b2, p);
      });
    });
  });
} // bencode::e::value()

static bool
pair(sp::Buffer &buf, const char *key, const dht::KeyValue *t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return bencode::e::list(buf, (void *)t, [](sp::Buffer &b, void *arg) {
    const auto *a = (const dht::KeyValue *)arg;
    return for_all(a, [&b](const dht::KeyValue &it) { //
      return value(b, it);
    });
  });
} // bencode::e::pair()

static bool
pair(sp::Buffer &buf, const char *key, const dht::StatTrafic &t) noexcept {
  // used by statistics
  if (!bencode::e::value(buf, key)) {
    return false;
  }
  return dict(buf, [&t](sp::Buffer &b) {
    if (!bencode::e::pair(b, "ping", t.ping)) {
      return false;
    }
    if (!bencode::e::pair(b, "find_node", t.find_node)) {
      return false;
    }
    if (!bencode::e::pair(b, "get_peers", t.get_peers)) {
      return false;
    }
    if (!bencode::e::pair(b, "announce_peer", t.announce_peer)) {
      return false;
    }
    if (!bencode::e::pair(b, "error", t.error)) {
      return false;
    }

    return true;
  });
}

static bool
pair(sp::Buffer &buf, const char *key, const dht::StatDirection &d) noexcept {
  // used by statistics
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return dict(buf, [&d](sp::Buffer &b) {
    if (!bencode::e::pair(b, "request", d.request)) {
      return false;
    }

    if (!bencode::e::pair(b, "response_timeout", d.response_timeout)) {
      return false;
    }

    if (!bencode::e::pair(b, "response", d.response)) {
      return false;
    }

    if (!bencode::e::pair(b, "parse_error", d.parse_error)) {
      return false;
    }
    // TODO
    return true;
  });
}

static bool
value(sp::Buffer &buf, const sp::list<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

static bool
pair(sp::Buffer &buf, const char *key, const sp::list<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
} // bencode::e::pair()

} // namespace e
} // namespace bencode

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
    if (!bencode::e::pair(b, "last_activity", dht.last_activity)) {
      return false;
    }
    if (!bencode::e::pair(b, "total_nodes", dht.total_nodes)) {
      return false;
    }
    if (!bencode::e::pair(b, "bad_nodes", dht.bad_nodes)) {
      return false;
    }
    if (!bencode::e::pair(b, "boostrap", dht.bootstrap_contacts)) {
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
    if (!bencode::e::pair(b, "sent", stat.sent)) {
      return false;
    }
    if (!bencode::e::pair(b, "received", stat.received)) {
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

} // namespace response

} // namespace krpc
