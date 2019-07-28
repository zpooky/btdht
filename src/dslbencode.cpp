#include "dslbencode.h"
#include <arpa/inet.h>
#include <heap/binary.h>
#include <util/assert.h>

//=BEncode==================================================================
namespace bencode {
/*bencode*/
namespace e {
/*bencode::e*/

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

// bool
// value_raw(sp::Buffer &b, const Contact &p) noexcept {
//   return serialize(b, p);
// } // bencode::e::value()

// bool
// pair(sp::Buffer &buf, const char *key, const Contact &p) noexcept {
//   if (!bencode::e::value(buf, key)) {
//     return false;
//   }
//
//   return bencode::e::value(buf, p);
// } // bencode::e::pair()

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
size(const Contact &p) noexcept {
  // TODO ipv4
  assertx(p.ip.type == IpType::IPV4);
  return sizeof(p.ip.ipv4) + sizeof(p.port);
}

static std::size_t
size(const dht::KContact &p) noexcept {
  return size(p.contact);
}

static std::size_t
size(const dht::Peer &p) noexcept {
  return size(p.contact);
}

static std::size_t
size(const dht::Node &p) noexcept {
  return sizeof(p.id.id) + size(p.contact);
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

template <typename T>
static std::size_t
size(const sp::dstack<T> &list) noexcept {
  std::size_t result = 0;
  sp::for_each(list, [&result](const T &ls) { //
    result += size(ls);
  });
  return result;
}

template <typename T>
static std::size_t
size(const heap::MaxBinary<T> &list) noexcept {
  std::size_t result = 0;
  for_each(list, [&result](const auto &ls) {
    //
    result += size(ls);
  });
  return result;
}

bool
pair_compact(sp::Buffer &buf, const char *key,
             const sp::UinArray<dht::Peer> &list) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  auto cb = [](sp::Buffer &b, void *arg) {
    auto &l = *((const sp::UinArray<dht::Peer> *)arg);

    return for_all(l, [&b](const auto &head) {
      if (!serialize(b, head.contact)) {
        return false;
      }

      return true;
    });
  };

  return bencode::e::value(buf, length(list), (void *)&list, cb);
} // bencode::e::pair_compact()

template <typename List>
static bool
sp_list(sp::Buffer &buf, const List &list) noexcept {
  std::size_t len = size(list); // TODO ??

  return bencode::e::value(buf, len, (void *)&list, [](sp::Buffer &b, void *a) {
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

bool
pair_compact(sp::Buffer &buf, const char *key, const dht::Node **list,
             std::size_t size) noexcept {
  return xxx(buf, key, list, size);
} // bencode::e::pair_compact()

//=====================================
namespace priv {
bool
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

bool
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

bool
value(sp::Buffer &buf, const dht::Bucket &t) noexcept {
  return list(buf, (void *)&t, [](sp::Buffer &b, void *arg) { //
    auto *a = (dht::Bucket *)arg;

    return for_all(*a, [&b](const dht::Node &node) { //
      return value(b, node);
    });
  });
}

bool
value(sp::Buffer &buf, const dht::RoutingTable &t) noexcept {
  // used by dump

  return dict(buf, [&t](sp::Buffer &b) {
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

bool
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

bool
pair(sp::Buffer &buf, const char *key, const dht::StatDirection &d) noexcept {
  // used by statistics
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return dict(buf, [&d](sp::Buffer &b) {
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
value(sp::Buffer &buf, const sp::list<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

bool
value(sp::Buffer &buf, const sp::dstack<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

bool
value(sp::Buffer &buf, const heap::MaxBinary<dht::KContact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

bool
pair(sp::Buffer &buf, const char *key, const sp::list<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
} // bencode::e::pair()

bool
pair(sp::Buffer &buf, const char *key, const sp::dstack<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
}

bool
pair(sp::Buffer &buf, const char *key,
     const heap::MaxBinary<dht::KContact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
}

} // namespace priv
} // namespace e

//=====================================
bool
d::priv::value(sp::Buffer &buf, Contact &res) noexcept {
  auto cb = [&res](auto &b) { //
    if (!bencode::d::pair(b, "ipv4", res.ip.ipv4)) {
      return false;
    }

    if (!bencode::d::pair(b, "port", res.port)) {
      return false;
    }

    return true;
  };

  if (!bencode::d::dict(buf, cb)) {
    return false;
  }

  return true;
}

bool
d::priv::value(sp::Buffer &buf, sp::UinArray<Contact> &res) noexcept {
  return bencode::d::list(buf, [&res](auto &b) {
    while (sp::remaining_read(b) > 0 && b[b.pos] != 'e') {
      Contact c;
      if (!bencode::d::priv::value(b, c)) {
        return false;
      }

      insert(res, c);

      return true;
    }
  });
}

//=====================================
} // namespace bencode
