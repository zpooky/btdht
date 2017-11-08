#ifndef SP_MAINLINE_DHT_BENCODE_H
#define SP_MAINLINE_DHT_BENCODE_H

#include "shared.h"

namespace bencode {

namespace e {
bool
value(sp::Buffer &, std::uint32_t) noexcept;
bool
value(sp::Buffer &, std::int32_t) noexcept;

bool
value(sp::Buffer &, const char *) noexcept;
bool
value(sp::Buffer &, const char *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
value(sp::Buffer &b, const char (&val)[SIZE]) noexcept {
  return value(b, val, SIZE);
}

bool
value(sp::Buffer &, const sp::byte *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
value(sp::Buffer &b, const sp::byte (&val)[SIZE]) noexcept {
  return value(b, val, SIZE);
}

bool
value(sp::Buffer &, const dht::NodeId &) noexcept;

bool
value(sp::Buffer &, const dht::Peer &) noexcept;

bool
value(sp::Buffer &, const dht::Node &) noexcept;

bool
list(sp::Buffer &, void *, bool (*)(sp::Buffer &, void *)) noexcept;

template <typename F>
static bool
dict(sp::Buffer &buffer, F f) noexcept {
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
pair(sp::Buffer &, const char *key, const char *value) noexcept;

bool
pair(sp::Buffer &, const char *key, std::uint32_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, const sp::byte *value,
     std::size_t length) noexcept;

template <std::size_t SIZE>
bool
pair(sp::Buffer &b, const char *key, const sp::byte (&value)[SIZE]) noexcept {
  return pair(b, key, value, SIZE);
}
} // namespace e

//----------------------------------
namespace d {
struct Decoder {
  explicit Decoder(sp::Buffer &);
};

template <typename F>
bool
dict(Decoder &p, F f) noexcept {
  return f(p);
}

bool
pair(Decoder &, const char *, char *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
pair(Decoder &p, const char *key, char (&value)[SIZE]) noexcept {
  return pair(p, key, value, SIZE);
}

bool
pair(Decoder &, const char *, sp::byte *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
pair(Decoder &p, const char *key, sp::byte (&value)[SIZE]) noexcept {
  return pair(p, key, value, SIZE);
}

bool
pair(Decoder &, const char *, bool &) noexcept;

bool
pair(Decoder &, const char *, std::uint32_t &) noexcept;

bool
pair(Decoder &, const char *, std::uint16_t &) noexcept;

bool
value(Decoder &, const char *) noexcept;

} // namespace d

} // namespace bencode

#endif
