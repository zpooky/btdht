#include "bencode_offset.h"
#include "bencode.h"
#include "bencode_print.h"
#include "util.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <util/assert.h>

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

bool
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
raw_compact_contact_v4(sp::Buffer &buf, Contact &contact) noexcept {
  const std::size_t pos = buf.pos;

  constexpr std::size_t cmp(sizeof(contact.ip.ipv4) + sizeof(contact.port));
  if (sp::remaining_read(buf) < cmp) {
    buf.pos = pos;
    return false;
  }

  std::memcpy(&contact.ip.ipv4, buf.raw + buf.pos, sizeof(contact.ip.ipv4));
  buf.pos += sizeof(contact.ip.ipv4);
  contact.ip.ipv4 = ntohl(contact.ip.ipv4);

  std::memcpy(&contact.port, buf.raw + buf.pos, sizeof(contact.port));
  buf.pos += sizeof(contact.port);
  contact.port = ntohs(contact.port);

  if (contact.port == 0 || contact.ip.ipv4 == 0) {
    // buf.pos = 0;
    // bencode_print(buf);
    // assertxs(peer.port != 0, peer.port, peer.ip.ipv4);
    // assertxs(peer.ip.ipv4 != 0, peer.port, peer.ip.ipv4);
    //
    // char bx[128] = {'\0'};
    // assertx_n(to_string(peer, bx));
    // printf("%s\n", bx);
    // return false;
    buf.pos = pos;
    return false;
  }

  return true;
} // bencode::d::raw_compact_contact_v4()

static bool
raw_compact_idcontact(sp::Buffer &buf, dht::IdContact &value) noexcept {
  Contact &contact = value.contact;
  const std::size_t pos = buf.pos;

  constexpr std::size_t cmp(sizeof(value.id.id));
  if (sp::remaining_read(buf) < cmp) {
    buf.pos = pos;
    return false;
  }

  std::memcpy(value.id.id, buf.raw + buf.pos, sizeof(value.id.id));
  buf.pos += sizeof(value.id.id);

  if (!raw_compact_contact_v4(buf, contact)) {
    buf.pos = pos;
    return false;
  }

  return true;
} // bencode::d::value()

template <typename ListType>
static bool
raw_compact_node_list(sp::Buffer &d, ListType &result) noexcept {
  const std::size_t pos = d.pos;

  const sp::byte *val = nullptr;
  std::size_t length = 0;
  if (!bencode::d::value_ref(d, /*OUT*/ val, /*OUT*/ length)) {
    d.pos = pos;
    return false;
  }

  // assertx(result.length == 0);

  if (length > 0) {
    assertx(val);

    if (length % (sizeof(dht::NodeId::id) + sizeof(Ipv4) + sizeof(Port)) != 0) {
      d.pos = pos;
      return false;
    }

    sp::Buffer val_buf((sp::byte *)val, length);
    val_buf.length = length;

    while (sp::remaining_read(val_buf) > 0) {
      typename ListType::value_type n;

      if (!raw_compact_idcontact(val_buf, n)) {
        //   d.pos = 0;
        //   dht::print_hex(d.raw, d.length);
        //   printf("\n");
        //   fprintf(stderr, "bo: \n");
        //   bencode_print_out(stderr);
        //   bencode_print(d);
        //   bencode_print_out(stdout);
        //   assertx(false);
        clear(result);
        d.pos = pos;
        return false;
      }

      insert(result, n);
    } // while
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

  assertxs(str, len);

  sp::Buffer bx((unsigned char *)str, len);
  bx.length = len;
  if (!bencode::d::raw_ip_or_ip_port(bx, result)) {
    b.pos = pos;
    return false;
  }

  if (remaining_read(bx) > 0) {
    printf("len[%zu], str[%.*s]\n", len, int(len), str);

    bx.pos = 0;
    bencode_print(bx);
    assertxs(false, remaining_read(bx));
  }

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
    //   assertx(false);
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
nodes(sp::Buffer &d, const char *key, sp::list<dht::IdContact> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

  if (!raw_compact_node_list(d, l)) {
    d.pos = pos;
    return false;
  }
  return true;
}

bool
nodes(sp::Buffer &d, const char *key,
      sp::UinStaticArray<dht::IdContact, 256> &l) noexcept {
  const std::size_t pos = d.pos;
  if (!bencode::d::value(d, key)) {
    d.pos = pos;
    return false;
  }

  if (!raw_compact_node_list(d, l)) {
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
