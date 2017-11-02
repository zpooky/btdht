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
bool
encode(sp::Buffer &, const sp::byte *, std::size_t) noexcept;

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
  buffer.raw[i++] = 'd';

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

bool
encodePair(sp::Buffer &, const char *key, const char *value) noexcept;

bool
encodePair(sp::Buffer &, const char *key, std::uint32_t value) noexcept;

bool
encodePair(sp::Buffer &, const char *key, const sp::byte *value,
           std::size_t length) noexcept;

//----------------------------------
bool
decode(sp::Buffer &, std::uint32_t &) noexcept;

bool
decode(sp::Buffer &, std::int32_t &) noexcept;

} // namespace bencode

#endif
