#include "priv_decode_bencode.h"
#include "decode_bencode.h"
#include <buffer/BytesView.h>
#include <buffer/Thing.h>
#include <cstring>
#include <util/conversions.h>

//=====================================
template <typename Buffer>
bool
bencode_priv_d<Buffer>::value(Buffer &buf,
                              sp::UinArray<Contact> &res) noexcept {
  return bencode_d<Buffer>::list(buf, [&res](auto &b) {
    while (remaining_read(b) > 0 && b[b.pos] != 'e') {
      Contact c{};
      if (!bencode_priv_d<Buffer>::value(b, c)) {
        return false;
      }

      insert(res, c);
    }
    return true;
  });
}

//=====================================
template <typename Buffer>
bool
bencode_priv_d<Buffer>::value(Buffer &buf, dht::IdContact &node) noexcept {
  return bencode_d<Buffer>::dict(buf, [&node](Buffer &b) { //
    if (!bencode_d<Buffer>::pair(b, "id", node.id)) {
      return false;
    }

    if (!bencode_priv_d<Buffer>::pair(b, "contact", node.contact)) {
      return false;
    }

    return true;
  });
}

//=====================================
template <typename Buffer>
bool
bencode_priv_d<Buffer>::value(Buffer &b, Contact &contact) noexcept {
  return bencode_d<Buffer>::dict(b, [&contact](Buffer &buffer) {
    if (!bencode_d<Buffer>::pair(buffer, "ipv4", contact.ip)) {
      return false;
    }
    // out.contact.ip = ntohl(ip);

    if (!bencode_d<Buffer>::pair(buffer, "port", contact.port)) {
      return false;
    }
    // out.contact.port = ntohs(out.contact.port);
    return true;
  });
} // bencode_d::value()

//=====================================
template <typename Buffer>
bool
bencode_priv_d<Buffer>::pair(Buffer &buf, const char *key,
                             Contact &p) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_priv_d<Buffer>::value(buf, p);
}

//=====================================
// template struct bencode_priv_d<sp::Thing>;
template struct bencode_priv_d<sp::BytesView>;

//=====================================
