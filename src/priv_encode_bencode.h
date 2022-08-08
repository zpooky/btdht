#ifndef SP_MAINLINE_DHT_PRIV_ENCODE_BENCODE_H
#define SP_MAINLINE_DHT_PRIV_ENCODE_BENCODE_H
#include "shared.h"
#include "util.h"

namespace sp {
namespace bencode {
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
};
} // namespace priv
} // namespace bencode
} // namespace sp

#endif
