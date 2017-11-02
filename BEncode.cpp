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

bool
encode(sp::Buffer &buffer, std::uint32_t in) noexcept {
  return encode_integer(buffer, "%u", in);
}

bool
encode(sp::Buffer &buffer, std::int32_t in) noexcept {
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
encode(sp::Buffer &buffer, const char *str) noexcept {
  return encode(buffer, str, std::strlen(str));
}

bool
encode(sp::Buffer &buffer, const char *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
}
bool
encode(sp::Buffer &buffer, const sp::byte *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
}

//-----------------------------
bool
encodeList(sp::Buffer &buffer, bool (*f)(sp::Buffer &)) noexcept {
  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;
  if (buffer.pos + 1 > buffer.capacity) {
    return false;
  }
  buffer.raw[i++] = 'l';

  if (!f(buffer)) {
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
generic_encodePair(sp::Buffer &buffer, const char *key, V value) {
  const std::size_t before = buffer.pos;
  if (!encode(buffer, key)) {
    buffer.pos = before;
    return false;
  }
  if (!encode(buffer, value)) {
    buffer.pos = before;
    return false;
  }
  return true;
}

bool
encodePair(sp::Buffer &buffer, const char *key, const char *value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
encodePair(sp::Buffer &buffer, const char *key, std::uint32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
encodePair(sp::Buffer &buffer, const char *key, const sp::byte *value,
           std::size_t length) noexcept {
  const std::size_t before = buffer.pos;
  if (!encode(buffer, key)) {
    buffer.pos = before;
    return false;
  }

  if (!encode(buffer, value, length)) {
    buffer.pos = before;
    return false;
  }
  return true;
}
//-----------------------------

bool
decode(sp::Buffer &b, std::uint32_t &out) noexcept {
  if (b.pos == b.length) {
    return false;
  }
  if (b[b.pos] != 'i') {
    return false;
  }
  // TODO
  return true;
}

bool
decode(sp::Buffer &, std::int32_t &) noexcept {
  // TODO
  return true;
}

} // namespace bencode
