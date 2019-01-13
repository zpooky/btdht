#include "decode_bencode.h"

#include <buffer/BytesView.h>
#include <buffer/Thing.h>
#include <cstring>

namespace sp {
//=====================================
template <typename Buffer>
bool
bencode::d<Buffer>::value(Buffer &b, std::uint64_t &val) noexcept {
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
bencode::d<Buffer>::value(Buffer &buf, byte *value, std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }

  return true;
}

static bool
value(Buffer &buf, char *value, std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }

  return true;
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

  out = std::atoll(str);
  return true;
}

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

// template struct bencode::d<sp::CircularByteBuffer>;
template struct bencode::d<sp::Thing>;
template struct bencode::d<sp::BytesView>;
} // namespace sp
