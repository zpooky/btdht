#include "bencode.h"
#include "bencode_offset.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

namespace bencode {
namespace d {

// i<integer encoded in base ten ASCII>e.
// <length>:<contents>
// l<contents>e
// d<contents>e

static bool
int_wildcard(sp::Buffer &d) noexcept {
  std::uint64_t val = 0;
  return bencode::d::value(d, val);
}

static bool
string_wildcard(sp::Buffer &d) noexcept {
  const char *val = nullptr;
  std::size_t len = 0;
  return bencode::d::value_ref(d, val, len);
}

static bool
list_wildcard(sp::Buffer &d) noexcept {
  const std::size_t pos = d.pos;
  if (d.raw[d.pos++] != 'l') {
    d.pos = pos;
    return false;
  }
Lretry:
  if (list_wildcard(d)) {
    goto Lretry;
  }
  if (int_wildcard(d)) {
    goto Lretry;
  }
  if (string_wildcard(d)) {
    goto Lretry;
  }
  if (dict_wildcard(d)) {
    goto Lretry;
  }
  if (d.raw[d.pos++] != 'e') {
    d.pos = pos;
    return false;
  }

  return true;
}

bool
dict_wildcard(sp::Buffer &d) noexcept {
  const std::size_t pos = d.pos;
  if (d.raw[d.pos++] != 'd') {
    d.pos = pos;
    return false;
  }
Lretry:
  if (list_wildcard(d)) {
    goto Lretry;
  }
  if (int_wildcard(d)) {
    goto Lretry;
  }
  if (string_wildcard(d)) {
    goto Lretry;
  }
  if (dict_wildcard(d)) {
    goto Lretry;
  }
  if (d.raw[d.pos++] != 'e') {
    d.pos = pos;
    return false;
  }

  return true;
}

static bool
value_to_peer(sp::Buffer &buf, Contact &peer) noexcept {
  const std::size_t pos = buf.pos;

  // TODO ipv4
  constexpr std::size_t cmp(sizeof(peer.ip.ipv4) + sizeof(peer.port));
  if (sp::remaining_read(buf) < cmp) {
    buf.pos = pos;
    return false;
  }

  // TODO ivp4
  std::memcpy(&peer.ip.ipv4, buf.raw + buf.pos, sizeof(peer.ip.ipv4));
  buf.pos += sizeof(peer.ip.ipv4);
  peer.ip.ipv4 = ntohl(peer.ip.ipv4);

  std::memcpy(&peer.port, buf.raw + buf.pos, sizeof(peer.port));
  buf.pos += sizeof(peer.port);
  peer.port = ntohs(peer.port);

  return true;
} // bencode::d::value_to_peer()

static bool
value(sp::Buffer &buf, dht::Node &value) noexcept {
  Contact &contact = value.contact;
  const std::size_t pos = buf.pos;

  constexpr std::size_t cmp(sizeof(value.id.id));
  if (sp::remaining_read(buf) < cmp) {
    buf.pos = pos;
    return false;
  }

  std::memcpy(value.id.id, buf.raw + buf.pos, sizeof(value.id.id));
  buf.pos += sizeof(value.id.id);

  if (!value_to_peer(buf, contact)) {
    buf.pos = pos;
    return false;
  }

  return true;
} // bencode::d::value()

template <typename ListType>
static bool
compact_list(sp::Buffer &d, const char *key, ListType &l) noexcept {
  const std::size_t pos = d.pos;

  const sp::byte *val = nullptr;
  std::size_t length = 0;
  if (!bencode::d::value_ref(d, val, length)) {
    d.pos = pos;
    return false;
  }

  // assert(l.length == 0);

  if (length > 0) {
    assert(val);

    sp::Buffer val_buf((sp::byte *)val, length);
    val_buf.length = length;
  Lcontinue:
    if (sp::remaining_read(val_buf) > 0) {
      typename ListType::value_type n;
      std::size_t pls = val_buf.pos;
      if (!value(val_buf, n)) {
        d.pos = pos;
        return false;
      }
      assert(val_buf.pos > pls);

      if (!insert(l, n)) {
        d.pos = pos;
        return false;
      }

      goto Lcontinue;
    }
  }

  return true;
}

static bool
value_contact(sp::Buffer &b, Contact &result) noexcept {
  const std::size_t pos = b.pos;

  const char *str = nullptr;
  std::size_t len = 0;
  if (!bencode::d::value_ref(b, str, len)) {
    b.pos = pos;
    return false;
  }

  sp::Buffer bx((unsigned char *)str, len);
  bx.length = len;
  if (!bencode::d::parse_convert(bx, result)) {
    b.pos = pos;
    return false;
  }
  assert(remaining_read(bx) == 0);

  return true;
}

template <typename ListType>
static bool
list_contact(sp::Buffer &d, const char *key, ListType &l) noexcept {
  static_assert(std::is_same<Contact, typename ListType::value_type>(), "");
  const std::size_t pos = d.pos;

  if (!internal::is(d, "l", 1)) {
    d.pos = pos;
    return false;
  }

  while (sp::remaining_read(d) > 0) {
    typename ListType::value_type n;

    const std::size_t itp = d.pos;
    if (!value_contact(d, n)) {
      d.pos = itp;
      break;
    }

    insert(l, n);
    // if (!insert(l, n)) {
    //   d.pos = pos;
    //   // too many result
    //   printf("length[%zu] capacity[%zu]\n", length(l), capacity(l));
    //   assert(false);
    //   return false;
    // }
  }

  if (!internal::is(d, "e", 1)) {
    d.pos = pos;
    return false;
  }

  return true;
}

bool
nodes(sp::Buffer &d, const char *key, sp::list<dht::Node> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

  if (!compact_list(d, key, l)) {
    d.pos = pos;
    return false;
  }
  return true;
}

bool
nodes(sp::Buffer &d, const char *key,
      sp::UinStaticArray<dht::Node, 256> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

  if (!compact_list(d, key, l)) {
    d.pos = pos;
    return false;
  }
  return true;
}

bool
peers(sp::Buffer &d, const char *key, sp::list<Contact> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

  if (!list_contact(d, key, l)) {
    d.pos = pos;
    return false;
  }
  return true;
}

bool
peers(sp::Buffer &d, const char *key,
      sp::UinStaticArray<Contact, 256> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

  if (!list_contact(d, key, l)) {
    d.pos = pos;
    return false;
  }

  return true;
}

} // namespace d
} // namespace bencode
