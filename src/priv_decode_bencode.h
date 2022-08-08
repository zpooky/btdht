#ifndef SP_MAINLINE_DHT_PRIV_DECODE_BENCODE_H
#define SP_MAINLINE_DHT_PRIV_DECODE_BENCODE_H

#include "shared.h"
#include "util.h"

// namespace bencode {
//=====================================
// namespace priv {
template <typename Buffer>
struct bencode_priv_d {

  //=====================================
  static bool
  value(Buffer &buf, sp::UinArray<Contact> &) noexcept;

  //=====================================
  static bool
  value(Buffer &buf, dht::IdContact &) noexcept;

  //=====================================
  static bool
  value(Buffer &, Contact &) noexcept;

  static bool
  pair(Buffer &, const char *key, Contact &p) noexcept;

  //=====================================
};
// } // namespace priv
// } // namespace bencode

#endif
