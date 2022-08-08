#include "priv_krpc.h"
// #include "bencode.h"
#include "cache.h"
#include "encode_bencode.h"
#include "krpc_shared.h"
#include <tree/bst_extra.h>

namespace krpc {
namespace priv {

//=====================================
bool
request::dump(sp::Buffer &b, const krpc::Transaction &t) noexcept {
  return req(b, t, "sp_dump", [](sp::Buffer &) { //
    return true;
  });
} // request::dump()

//=====================================
bool
request::statistics(sp::Buffer &b, const krpc::Transaction &t) noexcept {
  return req(b, t, "sp_statistics", [](sp::Buffer &) { //
    return true;
  });
}

//=====================================
bool
request::search(sp::Buffer &b, const krpc::Transaction &t,
                const dht::Infohash &search, std::size_t timeout) noexcept {
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
request::stop_search(sp::Buffer &b, const krpc::Transaction &t,
                     const dht::Infohash &search) noexcept {
  return req(b, t, "sp_search_stop", [&search](sp::Buffer &buffer) {
    /**/
    if (!bencode::e::pair(buffer, "search", search.id)) {
      return false;
    }
    return true;
  });
}

//=====================================
bool
response::dump(sp::Buffer &buf, const Transaction &t,
               const dht::DHT &dht) noexcept {
  return resp(buf, t, [&dht](auto &b) {
    if (!bencode::e::pair(b, "id", dht.id.id, sizeof(dht.id.id))) {
      return false;
    }

    if (!bencode::e::value(b, "cache")) {
      return false;
    }

    bool res = bencode::e::dict(b, [&dht](auto &b2) {
      if (!bencode::e::pair(b2, "min_read_idx", sp::cache_read_min_idx(dht))) {
        return false;
      }

      if (!bencode::e::pair(b2, "max_read_idx", sp::cache_read_max_idx(dht))) {
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

    if (!bencode::e::value(b, "db")) {
      return false;
    }

    res = bencode::e::dict(b, [&dht](auto &b2) {
      binary::rec::inorder(dht.db.lookup_table,
                           [&b2](dht::KeyValue &e) -> bool {
                             return bencode::e::dict(b2, [&e](auto &b3) {
                               char buffer[64]{0};
                               assertx_n(to_string(e.id, buffer));

                               if (!bencode::e::pair(b3, "infohash", buffer)) {
                                 return false;
                               }

                               std::uint64_t l(sp::n::length(e.peers));
                               if (!bencode::e::pair(b3, "entries", l)) {
                                 return false;
                               }

                               if (strlen(e.name) > 0) {
                                 if (!bencode::e::pair(b3, "name", e.name)) {
                                   return false;
                                 }
                               }

                               return true;
                             });
                           });
      return true;
    });

    if (!res) {
      return false;
    }

    if (!bencode::e::value(b, "ip_election")) {
      return false;
    }

    res = bencode::e::dict(b, [&dht](auto &b2) {
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
static bool
value(sp::Buffer &buf, const Contact &c) noexcept {
  return bencode::e::dict(buf, [&c](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "ipv4", c.ip.ipv4)) {
      return false;
    }
    if (!bencode::e::pair(b, "port", c.port)) {
      return false;
    }

    return true;
  });
}

static bool
value(sp::Buffer &buf, const dht::Node &node) noexcept {
  return bencode::e::dict(buf, [&node](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", node.id.id, sizeof(node.id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "good", node.good)) {
      return false;
    }

    if (!bencode::e::pair(b, "outstanding", node.outstanding)) {
      return false;
    }

    return true;
  });
}

static bool
value(sp::Buffer &buf, const dht::Bucket &t) noexcept {
  return bencode::e::list(buf, (void *)&t, [](sp::Buffer &b, void *arg) { //
    auto *a = (dht::Bucket *)arg;

    return for_all(*a, [&b](const dht::Node &node) { //
      return value(b, node);
    });
  });
}

static bool
value(sp::Buffer &buf, const dht::RoutingTable &t) noexcept {
  // used by dump

  return bencode::e::dict(buf, [&t](sp::Buffer &b) {
    // dht::Infohash id; // TODO
    // if (!pair(b, "id", id.id, sizeof(id.id))) {
    //   return false;
    // }

    if (!bencode::e::value(b, "bucket")) {
      return false;
    }

    if (!value(b, t.bucket)) {
      return false;
    }

    return true;
  });
} // bencode::e::value()

#if 0
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
    if (!bencode::e::value(b, "contact")) {
      return false;
    }
    if (!bencode::e::value(b, t.contact)) {
      return false;
    }

    if (!bencode::e::pair(b, "activity", std::uint64_t(t.activity))) {
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

#endif

static bool
serialize(sp::Buffer &b, const Contact &p) noexcept {
  // TODO ipv4
  assertx(p.ip.type == IpType::IPV4);
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
serialize(sp::Buffer &b, const dht::KContact &p) noexcept {
  return serialize(b, p.contact);
}

static bool
serialize(sp::Buffer &b, const dht::Node &node) noexcept {
  const std::size_t pos = b.pos;

  if (sp::remaining_write(b) < sizeof(node.id.id)) {
    b.pos = pos;
    return false;
  }
  std::memcpy(b.raw + b.pos, node.id.id, sizeof(node.id.id));
  b.pos += sizeof(node.id.id);

  if (!serialize(b, node.contact)) {
    return false;
  }

  return true;
} // bencode::e::serialize()

static std::size_t
serialize_size(const dht::KContact &p) noexcept {
  return serialize_size(p.contact);
}

static std::size_t
serialize_size(const dht::Node &p) noexcept {
  return sizeof(p.id.id) + serialize_size(p.contact);
}

static std::size_t
serialize_size(const dht::Node **list, std::size_t length) noexcept {
  std::size_t result = 0;
  for_all(list, length, [&result](const auto &value) { //
    result += serialize_size(value);
    return true;
  });
  return result;
}

template <typename T>
static std::size_t
serialize_size(const sp::list<T> &list) noexcept {
  std::size_t result = 0;
  sp::for_each(list, [&result](const T &ls) { //
    result += serialize_size(ls);
  });
  return result;
}

template <typename T>
static std::size_t
serialize_size(const sp::dstack<T> &list) noexcept {
  std::size_t result = 0;
  sp::for_each(list, [&result](const T &ls) { //
    result += serialize_size(ls);
  });
  return result;
}

template <typename T>
static std::size_t
serialize_size(const heap::MaxBinary<T> &list) noexcept {
  std::size_t result = 0;
  for_each(list, [&result](const auto &ls) {
    //
    result += serialize_size(ls);
  });
  return result;
}


template <typename List>
static bool
sp_list(sp::Buffer &buf, const List &list) noexcept {
  std::size_t sz = serialize_size(list);

  return bencode::e::value(buf, sz, (void *)&list, [](sp::Buffer &b, void *a) {
    const List *l = (List *)a;
    assertx(l);
    return for_all(*l, [&b](const auto &value) {
      //
      return serialize(b, value);
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

static bool
pair_compact(sp::Buffer &buf, const char *key,
             const sp::list<dht::Node> &list) noexcept {
  return internal_pair(buf, key, list);
} // bencode::e::pair()

static bool
pair_compact(sp::Buffer &buf, const char *key, const dht::Node **list,
             std::size_t sz) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  std::size_t len = serialize_size(list, sz);
  std::tuple<const dht::Node **, std::size_t> arg(list, sz);

  return bencode::e::value(buf, len, &arg, [](auto &b, void *a) {
    auto targ = (std::tuple<const dht::Node **, std::size_t> *)a;

    auto cb = [&b](const auto &value) {
      if (!serialize(b, value)) {
        return false;
      }

      return true;
    };

    return for_all(std::get<0>(*targ), std::get<1>(*targ), cb);
  });
}

static bool
value(sp::Buffer &buf, const sp::list<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

static bool
value(sp::Buffer &buf, const sp::dstack<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

static bool
value(sp::Buffer &buf, const heap::MaxBinary<dht::KContact> &t) noexcept {
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

static bool
pair(sp::Buffer &buf, const char *key, const sp::dstack<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
}

static bool
pair(sp::Buffer &buf, const char *key,
     const heap::MaxBinary<dht::KContact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
}

static bool
pair(sp::Buffer &buf, const char *key, const dht::StatTrafic &t) noexcept {
  // used by statistics
  if (!bencode::e::value(buf, key)) {
    return false;
  }
  return bencode::e::dict(buf, [&t](sp::Buffer &b) {
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

  return bencode::e::dict(buf, [&d](sp::Buffer &b) {
    if (!pair(b, "request", d.request)) {
      return false;
    }

    if (!pair(b, "response_timeout", d.response_timeout)) {
      return false;
    }

    if (!pair(b, "response", d.response)) {
      return false;
    }

    if (!bencode::e::pair(b, "parse_error", d.parse_error)) {
      return false;
    }
    // TODO
    return true;
  });
}

bool
response::statistics(sp::Buffer &buf, const Transaction &t,
                     const dht::Stat &stat) noexcept {
  return resp(buf, t, [&stat](auto &b) { //
    if (!pair(b, "sent", stat.sent)) {
      return false;
    }
    if (!pair(b, "received", stat.received)) {
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
response::search(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

bool
response::search_stop(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

bool
response::announce_this(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

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
        return value(b2, c);
      });
    };

    return bencode::e::list(b, (void *)&contacts, cb);
  });
}

template bool
found<sp::LinkedList<Contact>>(sp::Buffer &, const dht::Infohash &,
                               const sp::LinkedList<Contact> &) noexcept;
} // namespace event

} // namespace priv
} // namespace krpc
