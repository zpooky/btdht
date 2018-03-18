#ifndef SP_MAINLINE_DHT_BENCODE_H
#define SP_MAINLINE_DHT_BENCODE_H

#include "util.h"

namespace bencode {

//===============================================
namespace e {
bool
value(sp::Buffer &, std::uint32_t) noexcept;
bool
value(sp::Buffer &, std::int32_t) noexcept;

bool
value(sp::Buffer &, std::uint64_t) noexcept;
bool
value(sp::Buffer &, std::int64_t) noexcept;

bool
value(sp::Buffer &, const char *) noexcept;
bool
value(sp::Buffer &, const char *, std::size_t) noexcept;
bool
value(sp::Buffer &, std::size_t, void *,
      bool (*)(sp::Buffer &, void *)) noexcept;

bool
value(sp::Buffer &, const sp::byte *, std::size_t) noexcept;

bool
list(sp::Buffer &, void *, bool (*)(sp::Buffer &, void *)) noexcept;

template <typename F>
bool
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
} // bencode::e::dict

bool
pair(sp::Buffer &, const char *key, const char *value) noexcept;

template <std::size_t N>
bool
pair(sp::Buffer &b, const char *key, const char (&value)[N]) noexcept {
  return pair(b, key, value, N);
}

bool
pair(sp::Buffer &, const char *key, std::int16_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, std::uint16_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, std::int32_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, std::uint32_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, std::uint64_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, std::int64_t value) noexcept;

bool
pair(sp::Buffer &, const char *key, bool value) noexcept;

bool
pair(sp::Buffer &, const char *, const sp::byte *, std::size_t) noexcept;

template <std::size_t N>
bool
pair(sp::Buffer &b, const char *key, const sp::byte (&value)[N]) noexcept {
  return pair(b, key, value, N);
}
} // namespace e

//===============================================
namespace d {

namespace internal {
bool
is(sp::Buffer &, const char *, std::size_t) noexcept;

template <std::size_t SIZE>
bool
is(sp::Buffer &buf, const char (&exact)[SIZE]) {
  return is(buf, exact, SIZE);
}
} // namespace internal

template <typename F>
bool
dict(sp::Buffer &d, F f) noexcept {
  std::size_t pos = d.pos;
  if (!internal::is(d, "d", 1)) {
    d.pos = pos;
    return false;
  }

  if (!f(d)) {
    d.pos = pos;
    return false;
  }

  if (!internal::is(d, "e", 1)) {
    d.pos = pos;
    return false;
  }

  return true;
} // bencode::d::dict

bool
pair_x(sp::Buffer &, const char *, char *, /*IN&OUT*/ std::size_t &) noexcept;

template <std::size_t SIZE>
bool
pair(sp::Buffer &p, const char *key, char (&value)[SIZE]) noexcept {
  std::size_t length = SIZE;
  return pair_x(p, key, value, length);
} // bencode::d::pair()

template <std::size_t SIZE>
bool
pair(sp::Buffer &p, const char *key, char (&value)[SIZE],
     /*IN&OUT*/ std::size_t &length) noexcept {
  const std::size_t l = length;
  length = SIZE;

  if (!pair_x(p, key, value, length)) {
    length = l;
    return false;
  }
  return true;
} // bencode::d::pair()

bool
pair_x(sp::Buffer &, const char *, sp::byte *,
       /*IN&OUT*/ std::size_t &) noexcept;

template <std::size_t SIZE>
bool
pair(sp::Buffer &p, const char *key, sp::byte (&value)[SIZE]) noexcept {
  std::size_t length = SIZE;
  return pair_x(p, key, value, length);
} // bencode::d::pair()

template <std::size_t SIZE>
bool
pair(sp::Buffer &p, const char *key, sp::byte (&value)[SIZE],
     /*IN&OUT*/ std::size_t &length) noexcept {
  const std::size_t l = length;
  length = SIZE;

  if (!pair_x(p, key, value, length)) {
    length = l;
    return false;
  }
  return true;
} // bencode::d::pair()

bool
pair(sp::Buffer &, const char *, bool &) noexcept;

bool
pair(sp::Buffer &, const char *, std::uint64_t &) noexcept;

bool
pair(sp::Buffer &, const char *, std::uint32_t &) noexcept;

bool
pair(sp::Buffer &, const char *, std::uint16_t &) noexcept;

bool
pair(sp::Buffer &p, const char *key, dht::Token &) noexcept;

bool
pair(sp::Buffer &p, const char *key, Contact &) noexcept;

// bool
// pair(sp::Buffer &, const char *, sp::list<dht::Node> &) noexcept;

// bool
// pair(sp::Buffer &, const char *, sp::list<dht::Contact> &) noexcept;

bool
value(sp::Buffer &, const char *key) noexcept;

bool
value_ref(sp::Buffer &, const char *&, std::size_t &) noexcept;

bool
value_ref(sp::Buffer &, const sp::byte *&, std::size_t &) noexcept;

bool
value(sp::Buffer &, std::uint64_t &) noexcept;

bool
peek(const sp::Buffer &, const char *key) noexcept;

bool
pair_any(sp::Buffer &, char *, std::size_t, sp::byte *, std::size_t) noexcept;

template <std::size_t N, std::size_t N2>
bool
pair_any(sp::Buffer &d, char (&key)[N], sp::byte (&value)[N2]) noexcept {
  return pair_any(d, key, N, value, N2);
}

} // namespace d
} // namespace bencode

#endif
