#include "dht.h"

#include "bootstrap.h"
#include <hash/crc.h>
#include <prng/xorshift.h>

namespace dht {

template <std::size_t SIZE>
static bool
randomize(prng::xorshift32 &r, sp::byte (&buffer)[SIZE]) noexcept {
  sp::byte *it = buffer;
  std::size_t remaining = SIZE;

  while (remaining > 0) {
    auto val = random(r);
    std::size_t length = std::min(sizeof(val), remaining);

    std::memcpy(it, &val, length);
    remaining -= length;
    it += length;
  }
  return true;
}

// See http://www.rasterbar.com/products/libtorrent/dht_sec.html
static std::uint32_t
node_id_prefix(const Ip &addr, std::uint32_t seed) noexcept {
  sp::byte octets[8] = {0};
  std::uint32_t size = 0;
  if (addr.type == IpType::IPV6) {
#ifdef IP_IPV6
    // our external IPv6 address (network byte order)
    std::memcpy(octets, &addr.ipv6, sizeof(octets));
    // If IPV6
    constexpr sp::byte mask[] = {0x01, 0x03, 0x07, 0x0f,
                                 0x1f, 0x3f, 0x7f, 0xff};
    for (std::size_t i(0); i < sizeof(octets); ++i) {
      octets[i] &= mask[i];
    }
    size = sizeof(octets);
#else
    assertx(false);
#endif
  } else {
    // our external IPv4 address (network byte order)
    // TODO hton
    std::memcpy(octets, &addr.ipv4, sizeof(addr.ipv4));
    // 00000011 00001111 00111111 11111111
    constexpr sp::byte mask[] = {0x03, 0x0f, 0x3f, 0xff};
    for (std::size_t i = 0; i < sizeof(addr.ipv4); ++i) {
      octets[i] &= mask[i];
    }
    size = 4;
  }
  octets[0] |= sp::byte((seed << 5) & sp::byte(0xff));

  return crc32c::encode(octets, size);
}

// http://www.rasterbar.com/products/libtorrent/dht_sec.html
bool
is_valid_strict_id(const Ip &addr, const NodeId &id) noexcept {
  // TODO ipv4
  /*
   * TODO
   * if (is_ip_local(addr)) {
   *   return true;
   * }
   */
  std::uint32_t seed = id.id[19];
  std::uint32_t hash = node_id_prefix(addr, seed);
  // compare the first 21 bits only, so keep bits 17 to 21 only.
  sp::byte from_hash = sp::byte((hash >> 8) & 0xff);
  sp::byte from_node = id.id[2];
  return id.id[0] == sp::byte((hash >> 24) & 0xff) &&
         id.id[1] == sp::byte((hash >> 16) & 0xff) &&
         (from_hash & 0xf8) == (from_node & 0xf8);
}

// See http://www.rasterbar.com/products/libtorrent/dht_sec.html
bool
randomize_NodeId(prng::xorshift32 &r, const Ip &addr, NodeId &id) noexcept {
  // Lstart:
  std::uint32_t seed = random(r) & 0xff;
  std::uint32_t hash = node_id_prefix(addr, seed);
  id.id[0] = sp::byte((hash >> 24) & 0xff);
  id.id[1] = sp::byte((hash >> 16) & 0xff);
  id.id[2] = sp::byte((hash >> 8) & 0xff);
  // if(id.id[0] > 9){
  //   goto Lstart;
  // }
  // need to change all bits except the first 5, xor randomizes the rest of
  // the bits node_id[2] ^= static_cast<byte>(rand() & 0x7);
  for (std::size_t i = 3; i < 19; ++i) {
    id.id[i] = sp::byte(random(r) & 0xff);
  }
  id.id[19] = sp::byte(seed);

  // assertx(id.id[0] <= sp::byte(9));

  return true;
}

bool
init(dht::DHT &dht, const Options &o) noexcept {
  if (!randomize_NodeId(dht.random, dht.external_ip.ip, dht.id)) {
    return false;
  }

  for_each(o.bootstrap, [&](const Contact &cur) {
    bootstrap_insert(dht.bootstrap_meta, dht.bootstrap, dht::KContact(0, cur));
  });

  return true;
}

bool
is_blacklisted(const DHT &, const Contact &) noexcept {
  // XXX
  return false;
}

bool
should_mark_bad(const DHT &self, Node &contact) noexcept {
  // XXX
  return !is_good(self.routing_table, contact);
}

static std::size_t
shared_prefix(const dht::Key &a, const dht::Key &b) noexcept {
  std::size_t result = 0;
  for (std::size_t byte = 0; byte < sizeof(a); ++byte) {
    if (a[byte] == b[byte]) {
      result += 8;
    } else {
      for (std::size_t i = byte * 8; i < sizeof(a) * 8; ++i) {
        if (bit(a, i) == bit(b, i)) {
          ++result;
        } else {
          break;
        }
      }
      break;
    }
  }
  return result;
}

std::size_t
shared_prefix(const dht::Key &a, const dht::NodeId &b) noexcept {
  return shared_prefix(a, b.id);
}

std::size_t
shared_prefix(const dht::NodeId &a, const dht::NodeId &b) noexcept {
  return shared_prefix(a.id, b.id);
}

} // namespace dht
