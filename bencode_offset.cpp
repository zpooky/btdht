#include "bencode.h"
#include "bencode_offset.h"
#include <arpa/inet.h>
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

static bool
value(sp::Buffer &buf, Contact &value) noexcept {
  return value_to_peer(buf, value);
}

template <typename T>
static bool
compact_list(sp::Buffer &d, const char *key, sp::list<T> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

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
      typename sp::list<T>::value_type n;
      std::size_t pls = val_buf.pos;
      if (!value(val_buf, n)) {
        d.pos = pos;
        return false;
      }
      assert(val_buf.pos > pls);

      if (!sp::push_back(l, n)) {
        d.pos = pos;
        return false;
      }

      goto Lcontinue;
    }
  }

  return true;
}

bool
nodes(sp::Buffer &d, const char *key,
      sp::list<dht::Node> &l) noexcept {
  return compact_list(d, key, l);
}

bool
peers(sp::Buffer &d, const char *key, sp::list<Contact> &l) noexcept {
  return compact_list(d, key, l);
}

} // namespace d
} // namespace bencode
