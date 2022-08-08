#ifndef SP_MAINLINE_DHT_ENCODE_BENCODE_H
#define SP_MAINLINE_DHT_ENCODE_BENCODE_H

#include "shared.h"
#include "util.h"

namespace sp {
namespace bencode {

template <typename Buffer>
struct e {
  //=====================================
  static bool
  value(Buffer &, bool) noexcept;

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

  //=====================================
  static bool
  value(Buffer &, const char *str) noexcept;

  static bool
  value(Buffer &, const char *, std::size_t length) noexcept;

  static bool
  value(Buffer &, const sp::byte *, std::size_t length) noexcept;

  //=====================================
  static bool
  value_compact(Buffer &, const dht::Infohash *, std::size_t) noexcept;

  static bool
  value_id_contact_compact(Buffer &, const dht::Node **, std::size_t) noexcept;

  static bool
  value_id_contact_compact(Buffer &b, const sp::list<dht::Node> &list) noexcept;

  static bool
  value_compact(Buffer &b, const sp::UinArray<Contact> &list) noexcept;

  static bool
  value_compact(Buffer &buf, const sp::list<Contact> &list) noexcept;

  //=====================================
  static bool
  pair(Buffer &, const char *key, bool value) noexcept;

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

  //=====================================
  static bool
  pair(Buffer &, const char *key, const char *value) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const byte *value, std::size_t) noexcept;

  template <std::size_t SIZE>
  static bool
  pair(Buffer &buf, const char *key, byte (&value)[SIZE]) noexcept {
    return pair(buf, key, value, SIZE);
  } // bencode::d::pair()

  static bool
  pair_compact(Buffer &, const char *key, const dht::Infohash *,
               std::size_t) noexcept;
  static bool
  pair_id_contact_compact(Buffer &, const char *key, const dht::Node **,
                          std::size_t) noexcept;
  static bool
  pair_id_contact_compact(Buffer &, const char *key,
                          const sp::list<dht::Node> &list) noexcept;

  static bool
  pair_compact(Buffer &buf, const char *key,
               const sp::UinArray<Contact> &list) noexcept;

  static bool
  pair_compact(Buffer &buf, const char *key,
               const sp::list<Contact> &list) noexcept;

  //=====================================
  static bool
  list(Buffer &, void *, bool (*)(Buffer &, void *)) noexcept;

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

  // //=====================================
  // static bool
  // pair_compact(Buffer &, const char *key, const sp::UinArray<dht::Peer> &)
  // noexcept;
  //
  // //=====================================
  // static bool
  // value_compact(Buffer &, const dht::Node &) noexcept;
  //
  // static bool
  // value(Buffer &, const dht::Node &) noexcept;
  //
  // static bool
  // pair_compact(Buffer &, const char *, const sp::list<dht::Node> &) noexcept;
  //
  // static bool
  // pair_compact(Buffer &, const char *, const dht::Node **,
  //              std::size_t) noexcept;
  //
  //=====================================
  static bool
  value(Buffer &, std::size_t, void *, bool (*)(Buffer &, void *)) noexcept;

  //=====================================
}; // struct bencode::e

} // namespace bencode
} // namespace sp

#endif
