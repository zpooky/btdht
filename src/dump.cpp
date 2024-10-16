#include "decode_bencode.h"
#include "encode_bencode.h"

#include "dump.h"
#include <arpa/inet.h>

#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>
#include <buffer/Thing.h>
#include <io/file.h>
#include <util/assert.h>

namespace sp {
//========================
template <typename Buffer>
static bool
do_dump(Buffer &sink, const dht::DHT &dht, dht::RoutingTable *t) noexcept {
  return bencode::e<Buffer>::dict(sink, [&dht, t](Buffer &buffer) {
    if (!bencode::e<Buffer>::pair(buffer, "id", dht.id.id, sizeof(dht.id.id))) {
      return false;
    }

    Ip ip = dht.external_ip.ip;
    assertx(ip.type == IpType::IPV4);

    if (!bencode::e<Buffer>::pair(buffer, "ip", ip.ipv4)) {
      return false;
    }

    // if (!bencode::e<Buffer>::value(buffer, "nodes")) {
    //   return false;
    // }

    // auto cb = [](Buffer &b, /*ARG*/ void *a) {
    //   auto *table = (dht::RoutingTable *)a;
    //
    //   return for_all(table, [&b](auto &current) {
    //     /**/
    //     return for_all(current.bucket, [&b](auto &node) {
    //       return bencode::priv::e<Buffer>::value(b, node);
    //     });
    //   });
    // };
    //
    // return bencode::e<Buffer>::list(buffer, /*ARG*/ t, cb);
    return true;
  });
}

static bool
flush(CircularByteBuffer &b, void *arg) noexcept {
  assertx(arg);
  fd *f = (fd *)arg;

  if (!fs::write(*f, b)) {
    return false;
  }

  return true;
}

bool
dump(const dht::DHT &dht, const char *path) noexcept {
  // sp::StaticCircularByteBuffer<128> b;
  sp::StaticCircularByteBuffer<4096> b;

  fd file = fs::open_trunc(path);
  if (!file) {
    return false;
  }

  Sink sink(b, /*closure*/ &file, flush);

  if (!do_dump(sink, dht, dht.routing_table.root)) {
    return false;
  }

  if (!flush(sink)) {
    return false;
  }

  return true;
}

//========================
template <typename Buffer>
static bool
restore(Buffer &thing, /*OUT*/ dht::DHT &dht) noexcept {
  assertx(!is_marked(thing));

  return bencode_d<Buffer>::dict(thing, [&dht](Buffer &buffer) {
    assertx(!is_marked(buffer));
    if (!bencode_d<Buffer>::pair(buffer, "id", dht.id.id)) {
      return false;
    }

    Ip &ip = dht.external_ip.ip;
    assertx(!is_marked(buffer));
    if (!bencode_d<Buffer>::is_key(buffer, "ip")) {
      return false;
    }

    if (!bencode_d<Buffer>::value(buffer, ip.ipv4)) {
      return false;
    }

    assertx(!is_marked(buffer));
    // if (!bencode_d<Buffer>::value(buffer, "nodes")) {
    //   return false;
    // }
    //
    // assertx(!is_marked(buffer));
    // return bencode_d<Buffer>::list(buffer, [&bs, &dht](Buffer &b) {
    //   dht::IdContact contact;
    //   if (!bencode::priv::d<Buffer>::value(b, contact)) {
    //     return false;
    //   }
    //
    //   if (!insert_eager(bs, dht::KContact(contact, dht.id))) {
    //     assertxs(is_full(bs), length(bs), capacity(bs));
    //   }
    //
    //   return true;
    // });
    return true;
  });

  return true;
}

static bool
topup(sp::CircularByteBuffer &b, void *arg) noexcept {
  assertx(arg);
  fd *f = (fd *)arg;

  if (!fs::read(*f, b)) {
    return false;
  }

  return true;
}

bool
restore(dht::DHT &dht, const char *file) noexcept {
  fd f = fs::open_read(file);
  if (!f) {
    return true;
  }

  sp::StaticCircularByteBuffer<128> b;
  sp::Thing thing(b, &f, topup);

  if (!restore(thing, dht)) {
    return false;
  }

  return true;
}

//========================
} // namespace sp
