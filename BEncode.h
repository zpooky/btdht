#ifndef SP_MAINLINE_DHT_BENCODE_H
#define SP_MAINLINE_DHT_BENCODE_H

#include "shared.h"

namespace bencode {

bool
encode(sp::Buffer &, std::uint32_t) noexcept;
bool
encode(sp::Buffer &, std::int32_t) noexcept;

bool
encode(sp::Buffer &, const char *) noexcept;
bool
encode(sp::Buffer &, const char *, std::size_t) noexcept;
template <std::size_t SIZE>
bool
encode(sp::Buffer &, const char (&)[SIZE]) noexcept;
template <std::size_t SIZE>
bool
encode(sp::Buffer &, const char (&)[SIZE], std::size_t) noexcept;

bool
encodeList(sp::Buffer &, bool (*)(sp::Buffer &)) noexcept;

template <typename F>
static bool
encodeDict(sp::Buffer &buffer, F f) noexcept {
  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;
  if (buffer.pos + 1 > buffer.capacity) {
    return false;
  }
  buffer.start[i++] = 'd';

  if (!f(buffer)) {
    buffer.pos = before;
    return false;
  }

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer.start[i++] = 'e';

  return true;
}

bool
encodePair(sp::Buffer &, const char *, const char *) noexcept;

bool
encodePair(sp::Buffer &, const char *, std::uint32_t) noexcept;

//----------------------------------
bool
decode(sp::Buffer &, std::uint32_t &) noexcept;

bool
decode(sp::Buffer &, std::int32_t &) noexcept;

} // namespace bencode

#endif
