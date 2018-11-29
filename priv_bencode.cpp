#include "priv_bencode.h"
#include <arpa/inet.h>
#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>
#include <buffer/Thing.h>
#include <cstring>

//=BEncode==================================================================
namespace sp {

//=========================================================0
//=========================================================0
//=========================================================0
template <typename Buffer, typename T>
static bool
encode_numeric(Buffer &buffer, const char *format, T in) noexcept {
  char b[64] = {0};
  int res = std::snprintf(b, sizeof(b), format, in);
  if (res < 0) {
    return false;
  }

  if (!write(buffer, b, res)) {
    return false;
  }

  return true;
} // bencode::encode_numeric()

template <typename Buffer, typename T>
static bool
encode_integer(Buffer &buffer, const char *format, T in) noexcept {
  static_assert(std::is_integral<T>::value, "");

  if (!write(buffer, 'i')) {
    return false;
  }

  if (!encode_numeric(buffer, format, in)) {
    return false;
  }

  if (!write(buffer, 'e')) {
    return false;
  }

  return true;
} // bencode::encode_integer()

template <typename Buffer, typename T>
static bool
encode_raw(Buffer &buffer, const T *str, std::size_t length) noexcept {
  static_assert(
      std::is_same<T, char>::value || std::is_same<T, sp::byte>::value, "");

  if (!encode_numeric(buffer, "%zu", length)) {
    return false;
  }

  if (!write(buffer, ':')) {
    return false;
  }

  if (!write(buffer, str, length)) {
    return false;
  }

  return true;
} // bencode::e::encode_raw()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::uint16_t in) noexcept {
  return encode_integer(buffer, "%hu", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::int16_t in) noexcept {
  return encode_integer(buffer, "%h", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::uint32_t in) noexcept {
  return encode_integer(buffer, "%u", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::int32_t in) noexcept {
  return encode_integer(buffer, "%d", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::uint64_t in) noexcept {
  return encode_integer(buffer, "%llu", in);
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::int64_t in) noexcept {
  return encode_integer(buffer, "%lld", in);
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, const char *str) noexcept {
  return value(b, str, std::strlen(str));
}

// template <typename Buffer>
// bool
// bencode::e<Buffer>::value(Buffer &b, const unsigned char *str) noexcept {
//   return value(b, str, std::strlen(str));
// }

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, const char *str,
                          std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, const unsigned char *str,
                          std::size_t length) noexcept {
  return encode_raw(b, str, length);
} // bencode::e::value()

/*----------------------------------------------------------*/
template <typename Buffer, typename V>
static bool
generic_encodePair(Buffer &buffer, const char *key, V val) {

  if (!bencode::e<Buffer>::value(buffer, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value(buffer, val)) {
    return false;
  }

  return true;
} // bencode::e::generic_encodePair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         const char *value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::int16_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::uint16_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::int32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::uint32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::uint64_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::int64_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key, bool value) noexcept {
  return generic_encodePair(buffer, key, value);
}
/*----------------------------------------------------------*/

template <typename Buffer>
static bool
serialize(Buffer &b, const Contact &p) noexcept {
  // TODO ipv4
  assertx(p.ip.type == IpType::IPV4);

  return bencode::e<Buffer>::dict(b, [&p](Buffer &buffer) {
    Ipv4 ip = htonl(p.ip.ipv4);
    Port port = htons(p.port);

    {
      if (!bencode::e<Buffer>::value(buffer, "ip")) {
        return false;
      }
      if (!bencode::e<Buffer>::value(buffer, ip)) {
        return false;
      }
    }

    {
      if (!bencode::e<Buffer>::value(buffer, "port")) {
        return false;
      }
      if (!bencode::e<Buffer>::value(buffer, port)) {
        return false;
      }
    }
    return true;
  });
} // bencode::e::serialize()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, const Contact &p) noexcept {
  return serialize(b, p);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key,
                         const Contact &p) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::value(buf, p);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key, const byte *value,
                         std::size_t l) noexcept {

  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::value(buf, value, l);
}

template <typename Buffer>
bool
bencode::e<Buffer>::list(Buffer &buf, void *closure,
                         bool (*f)(Buffer &, void *)) noexcept {
  if (!write(buf, 'l')) {
    return false;
  }

  if (!f(buf, closure)) {
    return false;
  }

  if (!write(buf, 'e')) {
    return false;
  }

  return true;
}

template <typename Buffer>
static bool
serialize(Buffer &b, const dht::Node &node) noexcept {

  if (!write(b, node.id.id, sizeof(node.id.id))) {
    return false;
  }

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
  for_each(list, [&result](const T &ls) { //
    result += size(ls);
  });
  return result;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
                                 const dht::Peer *list) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  std::size_t len = size(list);
  return bencode::e<Buffer>::value(
      buf, len, (void *)list, [](Buffer &b, void *arg) {
        const dht::Peer *l = (dht::Peer *)arg;

        return dht::for_all(l, [&b](const auto &ls) {
          if (!serialize(b, ls.contact)) {
            return false;
          }

          return true;
        });
      });
} // bencode::e::pair_compact()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, std::size_t length, void *closure,
                          bool (*f)(Buffer &, void *)) noexcept {
  if (!encode_numeric(b, "%zu", length)) {
    return false;
  }

  if (!write(b, ':')) {
    return false;
  }

  if (!f(b, closure)) {
    return false;
  }

  return true;
}

template <typename Buffer, typename T>
static bool
sp_list(Buffer &buf, const sp::list<T> &list) noexcept {
  std::size_t len = size(list);
  return bencode::e<Buffer>::value(
      buf, len, (void *)&list, [](Buffer &b, void *a) {
        const sp::list<T> *l = (sp::list<T> *)a;
        assertx(l);
        return for_all(*l, [&b](const auto &value) {
          if (!serialize(b, value)) {
            return false;
          }
          return true;
        });
      });
}

template <typename Buffer, typename T>
static bool
internal_pair(Buffer &buf, const char *key, const sp::list<T> &list) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return sp_list(buf, list);
} // bencode::e::internal_pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
                                 const sp::list<dht::Node> &list) noexcept {
  return internal_pair(buf, key, list);
} // bencode::e::pair()

template <typename Buffer>
static bool
xxx(Buffer &buf, const char *key, const dht::Node **list,
    std::size_t sz) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  std::size_t len = size(list, sz);
  std::tuple<const dht::Node **, std::size_t> arg(list, sz);
  return bencode::e<Buffer>::value(buf, len, &arg, [](auto &b, void *a) {
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

template <typename Buffer>
bool
bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
                                 const dht::Node **list,
                                 std::size_t size) noexcept {
  return xxx(buf, key, list, size);
} // bencode::e::pair_compact()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buf, const dht::Node &node) noexcept {
  return bencode::e<Buffer>::dict(buf, [&node](Buffer &b) { //
    if (!bencode::e<Buffer>::pair(b, "id", node.id.id, sizeof(node.id.id))) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "good", node.good)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "ping_outstanding",
                                  node.ping_outstanding)) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buf, const dht::Bucket &t) noexcept {
  return bencode::e<Buffer>::list(buf, (void *)&t, [](Buffer &b, void *arg) { //
    auto *a = (dht::Bucket *)arg;

    return for_all(*a, [&b](const dht::Node &node) { //
      return value(b, node);
    });
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buf, const dht::RoutingTable &t) noexcept {
  // used by dump

  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) {
    // dht::Infohash id; // TODO
    // if (!pair(b, "id", id.id, sizeof(id.id))) {
    //   return false;
    // }

    if (!bencode::e<Buffer>::value(b, "bucket")) {
      return false;
    }

    if (!bencode::e<Buffer>::value(b, t.bucket)) {
      return false;
    }

    return true;
  });
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key,
                         const dht::RoutingTable *t) noexcept {
  // used by dump
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::list(buf, (void *)&t, [](Buffer &b, void *arg) { //
    const auto *a = (const dht::RoutingTable *)arg;

    return dht::for_all(a, [&b](const dht::RoutingTable &p) { //
      return value(b, p);
    });
  });
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buf, const dht::Peer &t) noexcept {
  // used by dump

  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) { //
    if (!bencode::e<Buffer>::value(b, "contact")) {
      return false;
    }
    if (!bencode::e<Buffer>::value(b, t.contact)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "activity", t.activity.value)) {
      return false;
    }

    return true;
  });
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buf, const dht::KeyValue &t) noexcept {
  // used by dump
  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) { //
    if (!bencode::e<Buffer>::pair(b, "id", t.id.id, sizeof(t.id.id))) {
      return false;
    }

    if (!bencode::e<Buffer>::value(b, "list")) {
      return false;
    }

    return bencode::e<Buffer>::list(
        b, (void *)&t, [](Buffer &b2, void *arg) { //
          const auto *a = (const dht::KeyValue *)arg;

          return for_all(a->peers, [&b2](const dht::Peer &p) { //
            return value(b2, p);
          });
        });
  });
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key,
                         const dht::KeyValue *t) noexcept {
  // used by dump
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::list(buf, (void *)t, [](Buffer &b, void *arg) {
    const auto *a = (const dht::KeyValue *)arg;
    return for_all(a, [&b](const dht::KeyValue &it) { //
      return value(b, it);
    });
  });
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key,
                         const dht::StatTrafic &t) noexcept {
  // used by statistics
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }
  return dict(buf, [&t](Buffer &b) {
    if (!bencode::e<Buffer>::pair(b, "ping", t.ping)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "find_node", t.find_node)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "get_peers", t.get_peers)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "announce_peer", t.announce_peer)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "error", t.error)) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key,
                         const dht::StatDirection &d) noexcept {
  // used by statistics
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return dict(buf, [&d](Buffer &b) {
    if (!bencode::e<Buffer>::pair(b, "request", d.request)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "response_timeout", d.response_timeout)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "response", d.response)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "parse_error", d.parse_error)) {
      return false;
    }
    // TODO
    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buf, const sp::list<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key,
                         const sp::list<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return value(buf, t);
} // bencode::e::pair()

//=========================================================0
//===Decode================================================0
//=========================================================0
template <typename Buffer, typename T>
static bool
read_numeric(Buffer &b, T &out, char end) noexcept {
  static_assert(std::is_integral<T>::value, "");

  char str[32] = {0};
  std::size_t it = 0;
Lloop : {
  unsigned char cur = 0;
  if (pop_front(b, cur) == 1) {
    if (cur != end) {

      if (cur >= '0' && cur <= '9') {
        if (it < sizeof(str)) {
          str[it++] = cur;
          goto Lloop;
        }
      }
      return false;
    }
  } else {
    assertx(false);
    return false;
  }
}

  if (it == 0) {
    return false;
  }

  out = std::atoll(str);
  return true;
} // bencode::d::read_numeric()

template <typename Buffer, typename T>
static bool
parse_string(Buffer &b, /*OUT*/ T *str, std::size_t N,
             std::size_t &len) noexcept {
  static_assert(sizeof(T) == 1, "");

  if (!read_numeric(b, len, ':')) {
    return false;
  }

  if (len > N) {
    assertx(false);
    return false;
  }

  if (!pop_front(b, str, len)) {
    return false;
  }

  return true;
}

template <typename Buffer, typename T, std::size_t N>
static bool
parse_string(Buffer &b, /*OUT*/ T (&str)[N], std::size_t &len) noexcept {
  return parse_string(b, str, N, len);
} // bencode::d::parse_string()

template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &buf, const char *key) noexcept {
  const auto key_len = std::strlen(key);

  unsigned char out[128] = {0};
  if (sizeof(out) < key_len) {
    assertx(false);
    return false;
  }
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, out, out_len)) {
    m.rollback = true;
    return false;
  }

  if (out_len == key_len && std::memcmp(key, out, key_len) == 0) {
    return true;
  }

  m.rollback = true;
  return false;
}

template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, std::uint32_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
    assertx(false);
    m.rollback = true;
    return false;
  }
  if (out != 'i') {
    m.rollback = true;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    m.rollback = true;
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, std::uint16_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
    assertx(false);
    m.rollback = true;
    return false;
  }
  if (out != 'i') {
    m.rollback = true;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    m.rollback = true;
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &buf, byte *value, std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }
  return true;
}

template <typename Buffer>
bool
bencode::d<Buffer>::pair(Buffer &buf, const char *key, byte *value,
                         std::size_t &l) noexcept {

  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value, l);
}

//=========================================================0
//=========================================================0
//=========================================================0
template struct bencode::e<sp::CircularByteBuffer>;
template struct bencode::e<sp::Sink>;

// template struct bencode::d<sp::CircularByteBuffer>;
template struct bencode::d<sp::Thing>;
} // namespace sp
