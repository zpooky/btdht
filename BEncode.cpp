#include "BEncode.h"
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace bencode {

template <typename T>
static bool
encode_numeric(sp::Buffer &buffer, const char *format, T in) noexcept {
  static_assert(std::is_integral<T>::value, "");

  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;

  if (buffer.pos + 1 > buffer.length) {
    buffer.pos = before;
    return false;
  }
  buffer[i++] = 'i';

  const std::size_t remaining = buffer.length - i;
  char *b = reinterpret_cast<char *>(buffer.start + i);
  int res = std::snprintf(b, remaining, format, in);
  if (res < 0) {
    buffer.pos = before;
    return false;
  }
  if (buffer.pos + res > buffer.length) {
    buffer.pos = before;
    return false;
  }
  i += res;

  if (buffer.pos + 1 > buffer.length) {
    buffer.pos = before;
    return false;
  }
  buffer[i++] = 'e';
  return true;
}

bool
encode(sp::Buffer &buffer, std::uint32_t in) noexcept {
  return encode_numeric(buffer, "%u", in);
}

bool
encode(sp::Buffer &buffer, std::int32_t in) noexcept {
  return encode_numeric(buffer, "%d", in);
}
//-----------------------------
bool
encode(sp::Buffer &buffer, const char *str) noexcept {
  return encode(buffer, str, std::strlen(str));
}

bool
encode(sp::Buffer &, const char *, std::size_t) noexcept {
  return false;
}

template <std::size_t SIZE>
bool
encode(sp::Buffer &, const char (&)[SIZE]) noexcept {
  return false;
}

template <std::size_t SIZE>
bool
encode(sp::Buffer &, const char (&)[SIZE], std::size_t) noexcept {
  return false;
}
//-----------------------------
bool
encodeList(sp::Buffer &, bool (*)(sp::Buffer &)) noexcept {
  return false;
}

} // namespace bencode
