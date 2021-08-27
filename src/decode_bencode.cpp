#include "decode_bencode.h"
#include <buffer/BytesView.h>
#include <buffer/Thing.h>
#include <cstring>
#include <util/conversions.h>

namespace sp {
//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, std::uint64_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
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
bencode::d<Buffer>::pair(Buffer &buf, const char *key,
                         std::uint64_t &value) noexcept {
  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, std::uint32_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
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
bencode::d<Buffer>::pair(Buffer &buf, const char *key,
                         std::uint32_t &value) noexcept {
  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, std::uint16_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
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
bencode::d<Buffer>::pair(Buffer &buf, const char *key,
                         std::uint16_t &value) noexcept {
  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value);
}

//=====================================

//=====================================
template <typename Buffer, typename T>
static bool
parse_string(Buffer &b, /*OUT*/ T *str, std::size_t N,
             std::size_t &len) noexcept {
  static_assert(sizeof(T) == 1, "");

  if (!read_numeric(b, len, ':')) {
    return false;
  }

  if (len > N) {
    // TODO we sometimes get 32byte "id" instead of the expected 20byte "id"
    return false;
  }

  len = pop_front(b, (unsigned char *)str, len);
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
bencode::d<Buffer>::value(Buffer &buf, byte *value, std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }
  len = out_len;

  return true;
}

template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &buf, char *value, std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }
  len = out_len;

  return true;
}

//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &buf, Ip &value) noexcept {
  return bencode::d<Buffer>::value(buf, value.ipv4);
}

template <typename Buffer>
bool
bencode::d<Buffer>::pair(Buffer &buf, const char *key, Ip &value) noexcept {
  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::pair(Buffer &buf, const char *key, byte *value,
                         std::size_t &l) noexcept {

  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value, l);
}

//=====================================
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

  return parse_int(str + 0, str + it, out);
}

//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, dht::NodeId &id) noexcept {
  auto len = sizeof(id.id);
  if (!value(b, id.id, len)) {
    return false;
  }

  if (len != 20) {
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::d<Buffer>::pair(Buffer &buf, const char *key,
                         dht::NodeId &value) noexcept {
  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::d<Buffer>::value(buf, value);
}

//=====================================
//====Private==========================
//=====================================
namespace bencode {
template <typename Buffer>
bool
priv::d<Buffer>::value(Buffer &buf, dht::IdContact &node) noexcept {
  return bencode::d<Buffer>::dict(buf, [&node](Buffer &b) { //
    if (!bencode::d<Buffer>::pair(b, "id", node.id)) {
      return false;
    }

    if (!bencode::priv::d<Buffer>::pair(b, "contact", node.contact)) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
priv::d<Buffer>::value(Buffer &b, Contact &contact) noexcept {
  return bencode::d<Buffer>::dict(b, [&contact](Buffer &buffer) {
    if (!bencode::d<Buffer>::pair(buffer, "ipv4", contact.ip)) {
      return false;
    }
    // out.contact.ip = ntohl(ip);

    if (!bencode::d<Buffer>::pair(buffer, "port", contact.port)) {
      return false;
    }
    // out.contact.port = ntohs(out.contact.port);
    return true;
  });
} // bencode::d::value()

template <typename Buffer>
bool
priv::d<Buffer>::pair(Buffer &buf, const char *key, Contact &p) noexcept {
  if (!bencode::d<Buffer>::value(buf, key)) {
    return false;
  }

  return priv::d<Buffer>::value(buf, p);
} // bencode::d::pair()
} // namespace bencode

//=====================================
// template struct bencode::d<sp::CircularByteBuffer>;
template struct bencode::d<sp::Thing>;
template struct bencode::d<sp::BytesView>;

template struct bencode::priv::d<sp::Thing>;
template struct bencode::priv::d<sp::BytesView>;

} // namespace sp
