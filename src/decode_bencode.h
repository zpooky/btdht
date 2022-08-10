#ifndef SP_MAINLINE_DHT_DECODE_BENCODE_H
#define SP_MAINLINE_DHT_DECODE_BENCODE_H

#include "shared.h"
#include "util.h"
#include <string>

//=====================================
template <typename Buffer>
struct bencode_d {
  //=====================================
  static bool
  value(Buffer &buf, std::uint64_t &) noexcept;

  static bool
  pair(Buffer &buf, const char *key, std::uint64_t &) noexcept;
  //=====================================
  static bool
  value(Buffer &buf, std::uint32_t &) noexcept;

  static bool
  pair(Buffer &buf, const char *key, std::uint32_t &) noexcept;
  //=====================================
  static bool
  value(Buffer &buf, std::uint16_t &) noexcept;

  static bool
  pair(Buffer &buf, const char *key, std::uint16_t &) noexcept;
  //=====================================
  static bool
  is_key(Buffer &buf, const char *key) noexcept;

  static bool
  value(Buffer &buf, sp::byte *value, std::size_t &) noexcept;

  static bool
  value(Buffer &buf, char *value, std::size_t &) noexcept;

  static bool
  value(Buffer &buf, sp::UinArray<std::string> &) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, Ip &) noexcept;

  static bool
  pair(Buffer &buf, const char *, Ip &) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, std::string &) noexcept;

  //=====================================
  static bool
  pair(Buffer &buf, const char *key, sp::byte *value, std::size_t &) noexcept;

  template <std::size_t SIZE>
  static bool
  pair(Buffer &buf, const char *key, sp::byte (&value)[SIZE]) noexcept {
    auto len = SIZE;
    return pair(buf, key, value, len);
  } // bencode::d::pair()

  //=====================================
  static bool
  value(Buffer &, dht::NodeId &) noexcept;

  static bool
  pair(Buffer &, const char *, dht::NodeId &) noexcept;

private:
  //=====================================
  template <std::size_t N>
  static bool
  is(Buffer &buf, const char (&exact)[N], std::size_t length) noexcept {
    assertx(N >= length);

    unsigned char e[N] = {0};
    auto m = mark(buf);

    auto res = pop_front(buf, e, length);
    if (res != length) {
      m.rollback = true;
      return false;
    }

    return std::memcmp(e, exact, length) == 0;
  }

public:
  //=====================================
  template <typename F>
  static bool
  dict(Buffer &b, F f) noexcept;

  template <typename F>
  static bool
  list(Buffer &buf, F f) noexcept;

  static bool
  pair(Buffer &buf, const char *key, sp::UinArray<std::string> &) noexcept;

  //=====================================
  static bool
  value_compact(Buffer &buf, sp::UinArray<dht::Infohash> &) noexcept;

  static bool
  value_compact(Buffer &buf,
                sp::UinArray<std::tuple<dht::NodeId, Contact>> &) noexcept;
}; // struct bencode::d

template <typename Buffer>
template <typename F>
bool
bencode_d<Buffer>::dict(Buffer &b, F f) noexcept {
  if (!is(b, "d", 1)) {
    return false;
  }

  if (!f(b)) {
    return false;
  }

  if (!is(b, "e", 1)) {
    return false;
  }

  return true;
}

template <typename Buffer>
template <typename F>
bool
bencode_d<Buffer>::list(Buffer &buf, F f) noexcept {
  {
    auto m = mark(buf);
    unsigned char out = '\0';
    if (pop_front(buf, out) != 1) {
      m.rollback = true;
      return false;
    }

    if (out != 'l') {
      m.rollback = true;
      return false;
    }
  }

  {
  Lit:
    unsigned char out = '\0';
    if (peek_front(buf, out) != 1) {
      return false;
    }

    if (out == 'e') {
      assertx(pop_front(buf, out) == 1);
      assertx(out == 'e');
      return true;
    } else {
      if (!f(buf)) {
        return false;
      }
      goto Lit;
    }
  }

  return true;
}

#endif
