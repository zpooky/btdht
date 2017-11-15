#include "BEncode.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace bencode {

template <typename T>
static bool
encode_numeric(sp::Buffer &buffer, const char *format, T in) noexcept {
  std::size_t &i = buffer.pos;
  const std::size_t remaining = buffer.capacity - i;

  char *b = reinterpret_cast<char *>(buffer.raw + i);
  int res = std::snprintf(b, remaining, format, in);
  if (res < 0) {
    return false;
  }
  if (buffer.pos + res > buffer.capacity) {
    return false;
  }
  i += res;
  return true;
}

template <typename T>
static bool
encode_integer(sp::Buffer &buffer, const char *format, T in) noexcept {
  static_assert(std::is_integral<T>::value, "");

  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer[i++] = 'i';

  if (!encode_numeric(buffer, format, in)) {
    buffer.pos = before;
    return false;
  }

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer[i++] = 'e';
  return true;
}
namespace e {
bool
value(sp::Buffer &buffer, std::uint32_t in) noexcept {
  return encode_integer(buffer, "%u", in);
}

bool
value(sp::Buffer &buffer, std::int32_t in) noexcept {
  return encode_integer(buffer, "%d", in);
}
//-----------------------------
template <typename T>
bool
encode_raw(sp::Buffer &buffer, const T *str, std::size_t length) noexcept {
  const std::size_t before = buffer.pos;
  if (!encode_numeric(buffer, "%u", length)) {
    buffer.pos = before;
    return false;
  }
  if ((buffer.pos + length + 1) > buffer.capacity) {
    buffer.pos = before;
    return false;
  }

  std::size_t &i = buffer.pos;
  buffer[i++] = ':';
  std::memcpy(buffer.raw + i, str, length);
  i += length;

  return true;
}

bool
value(sp::Buffer &buffer, const char *str) noexcept {
  return value(buffer, str, std::strlen(str));
}

bool
value(sp::Buffer &buffer, const char *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
}

bool
value(sp::Buffer &buffer, const sp::byte *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
}

//-----------------------------
bool
list(sp::Buffer &buffer, void *arg, bool (*f)(sp::Buffer &, void *)) noexcept {
  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;
  if (buffer.pos + 1 > buffer.capacity) {
    return false;
  }
  buffer.raw[i++] = 'l';

  if (!f(buffer, arg)) {
    buffer.pos = before;
    return false;
  }

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer.raw[i++] = 'e';

  return true;
}

//-----------------------------

template <typename V>
static bool
generic_encodePair(sp::Buffer &buffer, const char *key, V val) {
  const std::size_t before = buffer.pos;
  if (!value(buffer, key)) {
    buffer.pos = before;
    return false;
  }
  if (!value(buffer, val)) {
    buffer.pos = before;
    return false;
  }
  return true;
}

bool
pair(sp::Buffer &buffer, const char *key, const char *value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &buffer, const char *key, std::uint32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &buffer, const char *key, const sp::byte *val,
     std::size_t length) noexcept {
  const std::size_t before = buffer.pos;
  if (!value(buffer, key)) {
    buffer.pos = before;
    return false;
  }

  if (!value(buffer, val, length)) {
    buffer.pos = before;
    return false;
  }
  return true;
}
} // namespace e
//-----------------------------

namespace d {
template <typename T>
static bool
read_numeric(sp::Buffer &b, T &out, char end) noexcept {
  static_assert(std::is_integral<T>::value, "");
  char str[32] = {0};
  std::size_t it = 0;
Lloop:
  if (sp::remaining_read(b) > 0) {
    if (b[b.pos] != end) {

      if (b[b.pos] >= 0 && b[b.pos] <= 9) {
        if (it < sizeof(str)) {
          str[it++] = b[b.pos++];
          goto Lloop;
        }
      }
      return false;
    }
  } else {
    return false;
  }

  if (it == 0) {
    return false;
  }

  out = std::atoll(str);
  return true;
}

template <typename T>
static bool
parse_string(sp::Buffer &b, /*OUT*/ const T *&str, std::size_t &len) noexcept {
  static_assert(sizeof(T) == 1, "");
  if (!read_numeric(b, len, ':')) {
    return false;
  }
  if (len > sp::remaining_read(b)) {
    return false;
  }
  str = (T *)b.raw;
  b.pos += len;

  return true;
}

static bool
parse_key(sp::Buffer &b, const char *key) noexcept {
  const std::size_t key_len = std::strlen(key);

  const char *parse_key = nullptr;
  std::size_t parse_key_len = 0;
  if (!parse_string(b, parse_key, parse_key_len)) {
    return false;
  }

  if (key_len != parse_key_len || std::memcmp(key, parse_key, key_len) != 0) {
    return false;
  }

  return true;
}

template <typename T>
static bool
parse_key_value(sp::Buffer &b, const char *key, T *val,
                std::size_t len) noexcept {
  if (!parse_key(b, key)) {
    return false;
  }

  const sp::byte *val_ref = nullptr;
  std::size_t val_len = 0;
  if (!parse_string(b, val_ref, val_len)) {
    return false;
  }
  if (val_len > len) {
    return false;
  }

  std::memcpy(val, val_ref, val_len);
  // TODO howd to indicate length of parsed val?

  return true;
}

static bool
parse_key_valuex(sp::Buffer &b, const char *key, std::uint64_t &val) noexcept {
  if (!parse_key(b, key)) {
    return false;
  }

  if (sp::remaining_read(b) == 0 || b.raw[b.pos++] != 'i') {
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    return false;
  }

  return true;
}

/*Decoder*/
Decoder::Decoder(sp::Buffer &b)
    : buf(b) {
}

bool
pair(Decoder &d, const char *key, char *val, std::size_t len) noexcept {
  return parse_key_value(d.buf, key, val, len);
}

bool
pair(Decoder &d, const char *key, sp::byte *val, std::size_t len) noexcept {
  return parse_key_value(d.buf, key, val, len);
}

bool
pair(Decoder &d, const char *key, bool &v) noexcept {
  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    return false;
  }

  v = t == 1;

  return true;
}

bool
pair(Decoder &d, const char *key, std::uint32_t &v) noexcept {
  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    return false;
  }

  if (t > std::uint64_t(~std::uint32_t(0))) {
    return false;
  }

  v = std::uint32_t(t);

  return true;
}

bool
pair(Decoder &d, const char *key, std::uint16_t &v) noexcept {
  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    return false;
  }

  if (t > std::uint64_t(~std::uint16_t(0))) {
    return false;
  }

  v = std::uint16_t(t);

  return true;
}

bool
value(Decoder &d, const char *key) noexcept {
  return parse_key(d.buf, key);
}

} // namespace d

} // namespace bencode
