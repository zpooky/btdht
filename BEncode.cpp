#include "BEncode.h"
#include <cstdio>
#include <cstring>
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
/*Decoder*/
Decoder::Decoder(sp::Buffer &) {
}

bool
pair(Decoder &, const char *, sp::byte *, std::size_t) noexcept {
  // TODO
  return true;
}

bool
pair(Decoder &, const char *, char *, std::size_t) noexcept {
  // TODO
  return true;
}

bool
pair(Decoder &, const char *) noexcept {
  // TODO
  return true;
}

bool
pair(Decoder &, const char *, bool &) noexcept {
  // TODO
  return true;
}

bool
pair(Decoder &, const char *, std::uint32_t &) noexcept {
  // TODO
  return true;
}

bool
pair(Decoder &, const char *, std::uint16_t &) noexcept {
  // TODO
  return true;
}

bool
value(Decoder &, const char *) noexcept {
  // TODO
  return true;
}

} // namespace d

} // namespace bencode
