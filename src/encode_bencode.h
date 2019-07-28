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
  // pair_compact(Buffer &, const char *key, const sp::UinArray<dht::Peer> &) noexcept;
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

namespace priv {
template <typename Buffer>
struct e {
  //=====================================
  static bool
  value(Buffer &, const dht::NodeId &) noexcept;

  static bool
  pair(Buffer &, const char *key, const dht::NodeId &) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, const dht::Node &) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, const dht::Bucket &t) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, const dht::RoutingTable &) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const dht::RoutingTable *) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, const dht::Peer &t) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, const dht::KeyValue &t) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const dht::KeyValue &t) noexcept;

  //=====================================
  static bool
  pair(Buffer &buf, const char *key, const dht::StatTrafic &t) noexcept;

  static bool
  pair(Buffer &buf, const char *key, const dht::StatDirection &d) noexcept;

  //=====================================
  static bool
  value(Buffer &, const Contact &) noexcept;

  static bool
  pair(Buffer &, const char *key, const Contact &p) noexcept;

  //=====================================
}; // struct bencode::e
} // namespace priv

} // namespace bencode
} // namespace sp

#endif
