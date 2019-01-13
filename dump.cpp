#include "encode_bencode.h"
#include "decode_bencode.h"

#include "dump.h"
#include <arpa/inet.h>

#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>
#include <buffer/Thing.h>
#include <io/file.h>
#include <util/assert.h>

namespace sp {

template <typename Buffer>
static bool
do_dump(Buffer &sink, const dht::DHT &dht, dht::RoutingTable *t) noexcept {
  return bencode::e<Buffer>::dict(sink, [&dht, t](Buffer &buffer) {
    if (!bencode::e<Buffer>::pair(buffer, "id", dht.id.id, sizeof(dht.id.id))) {
      return false;
    }

    Ip ip = dht.ip.ip;
    assertx(ip.type == IpType::IPV4);

    if (!bencode::e<Buffer>::pair(buffer, "ip", ip.ipv4)) {
      return false;
    }

    if (!bencode::e<Buffer>::value(buffer, "nodes")) {
      return false;
    }

    return bencode::e<Buffer>::list(
        buffer, /*ARG*/ t, [](Buffer &b, /*ARG*/ void *a) {
          auto *table = (dht::RoutingTable *)a;
          /**/
          return for_all(table, [&b](auto &current) {
            /**/
            return for_all(current.bucket, [&b](auto &node) {
              return bencode::e<Buffer>::value(b, node);
            });
          });
        });

    // TODO bootstrap
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

  if (!do_dump(sink, dht, dht.root)) {
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
restore_node(Buffer &b, dht::Node &out) {
  assertx(!is_marked(b));
  return bencode::d<Buffer>::dict(b, [&out](Buffer &buffer) {
    {
      assertx(!is_marked(buffer));
      if (!bencode::d<Buffer>::value(buffer, "id")) {
        return false;
      }

      if (!bencode::d<Buffer>::value(buffer, out.id)) {
        return false;
      }
    }
    auto &contact = out.contact;
    {
      assertx(!is_marked(buffer));
      if (!bencode::d<Buffer>::value(buffer, "ip")) {
        return false;
      }

      assertx(!is_marked(buffer));
      Ipv4 ip = 0;
      if (!bencode::d<Buffer>::value(buffer, ip)) {
        return false;
      }
      contact.ip = ntohl(ip);
    }

    {
      assertx(!is_marked(buffer));
      if (!bencode::d<Buffer>::value(buffer, "port")) {
        return false;
      }
      assertx(!is_marked(buffer));
      if (!bencode::d<Buffer>::value(buffer, contact.port)) {
        return false;
      }
      contact.port = ntohs(contact.port);
    }

    assertx(!is_marked(buffer));
    return true;
  });
}

template <typename Buffer, typename Bootstrap>
static bool
restore(Buffer &thing, /*OUT*/ dht::DHT &dht,
        /*OUT*/ Bootstrap &bs) noexcept {
  assertx(!is_marked(thing));

  return bencode::d<Buffer>::dict(thing, [&dht, &bs](Buffer &buffer) {
    assertx(!is_marked(buffer));
    if (!bencode::d<Buffer>::pair(buffer, "id", dht.id.id)) {
      return false;
    }

    Ip &ip = dht.ip.ip;
    assertx(!is_marked(buffer));
    if (!bencode::d<Buffer>::value(buffer, "ip")) {
      return false;
    }

    if (!bencode::d<Buffer>::value(buffer, ip.ipv4)) {
      return false;
    }

    assertx(!is_marked(buffer));
    if (!bencode::d<Buffer>::value(buffer, "nodes")) {
      return false;
    }

    assertx(!is_marked(buffer));
    return bencode::d<Buffer>::list(buffer, [&bs, &dht](Buffer &b) {
      dht::Node contact;
      if (!restore_node(b, contact)) {
        return false;
      }

      if (!insert_eager(bs, dht::KContact(contact, dht.id))) {
        assertx(false);
        return false;
      }

      return true;
    });
  });

  return true;
} // namespace sp

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

  if (!restore(thing, dht, dht.bootstrap)) {
    return false;
  }

  return true;
}

} // namespace sp
