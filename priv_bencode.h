#ifndef SP_MAINLINE_DHT_PRIV_BENCODE_H
#define SP_MAINLINE_DHT_PRIV_BENCODE_H

#include "shared.h"
#include "util.h"

namespace sp {
namespace bencode {
template <typename Buffer>
struct e {

  /*----------------------------------------------------------*/
  static bool
  value(Buffer &, std::uint16_t) noexcept;
  static bool
  value(Buffer &, std::int16_t) noexcept;

  static bool
  value(Buffer &, std::uint32_t) noexcept;
  static bool
  value(Buffer &, std::int32_t) noexcept;

  static bool
  value(Buffer &, std::uint64_t) noexcept;
  static bool
  value(Buffer &, std::int64_t) noexcept;

  static bool
  value(Buffer &, const char *) noexcept;

  // static bool
  // value(Buffer &buf, const unsigned char *str) noexcept;

  static bool
  value(Buffer &buffer, const char *str, std::size_t length) noexcept;

  static bool
  value(Buffer &b, const unsigned char *str, std::size_t length) noexcept;

  /*----------------------------------------------------------*/
  static bool
  pair(Buffer &, const char *key, const char *value) noexcept;

  static bool
  pair(Buffer &, const char *key, std::int16_t value) noexcept;

  static bool
  pair(Buffer &, const char *key, std::uint16_t value) noexcept;

  static bool
  pair(Buffer &, const char *key, std::int32_t value) noexcept;

  static bool
  pair(Buffer &, const char *key, std::uint32_t value) noexcept;

  static bool
  pair(Buffer &, const char *key, std::uint64_t value) noexcept;

  static bool
  pair(Buffer &, const char *key, std::int64_t value) noexcept;

  static bool
  pair(Buffer &, const char *key, bool value) noexcept;
  /*----------------------------------------------------------*/

  static bool
  value(Buffer &b, const Contact &p) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const Contact &p) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const byte *value, std::size_t) noexcept;

  template <std::size_t SIZE>
  static bool
  pair(Buffer &buf, const char *key, byte (&value)[SIZE]) noexcept {
    return pair(buf, key, value, SIZE);
  } // bencode::d::pair()

  static bool
  list(Buffer &, void *, bool (*)(Buffer &, void *)) noexcept;

  static bool
  pair_compact(Buffer &, const char *, const Contact *list) noexcept;

  static bool
  pair_compact(Buffer &buf, const char *key, const dht::Peer *list) noexcept;

  static bool
  pair_compact(Buffer &, const char *, const sp::list<dht::Node> &) noexcept;

  static bool
  pair_compact(Buffer &, const char *, const dht::Node **,
               std::size_t) noexcept;

  static bool
  value(Buffer &, std::size_t, void *, bool (*)(Buffer &, void *)) noexcept;

  template <typename F>
  static bool
  dict(Buffer &b, F f) noexcept {
    if (!write(b, 'd')) {
      return false;
    }

    if (!f(b)) {

      return false;
    }

    if (!write(b, 'e')) {
      return false;
    }

    return true;
  }

  // struct priv {
  static bool
  value(Buffer &buf, const dht::Node &) noexcept;

  static bool
  value(Buffer &buf, const dht::Bucket &t) noexcept;

  static bool
  value(Buffer &buf, const dht::RoutingTable &) noexcept;
  static bool
  pair(Buffer &buf, const char *key, const dht::RoutingTable *) noexcept;

  static bool
  value(Buffer &buf, const dht::Peer &t) noexcept;

  static bool
  value(Buffer &buf, const dht::KeyValue &t) noexcept;
  static bool
  pair(Buffer &buf, const char *key, const dht::KeyValue *t) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const dht::StatTrafic &t) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const dht::StatDirection &d) noexcept;

  static bool
  value(Buffer &buf, const sp::list<Contact> &t) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const sp::list<Contact> &t) noexcept;
  // };
}; // struct bencode::e

template <typename Buffer>
struct d {
private:
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
  static bool
  value(Buffer &buf, const char *key) noexcept;

  static bool
  value(Buffer &buf, std::uint32_t &) noexcept;

  static bool
  value(Buffer &buf, std::uint16_t &) noexcept;

  static bool
  value(Buffer &buf, byte *value, std::size_t &) noexcept;

  static bool
  pair(Buffer &buf, const char *key, byte *value, std::size_t &) noexcept;

  template <std::size_t SIZE>
  static bool
  pair(Buffer &buf, const char *key, byte (&value)[SIZE]) noexcept {
    auto len = SIZE;
    return pair(buf, key, value, len);
  } // bencode::d::pair()

  template <typename F>
  static bool
  list(Buffer &buf, F f) noexcept {
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

  /**/
  template <typename F>
  static bool
  dict(Buffer &b, F f) noexcept {
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
}; // struct bencode::d

} // namespace bencode
} // namespace sp

#endif
