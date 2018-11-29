#include "priv_bencode.h"

#include "dump.h"
#include <arpa/inet.h>

#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>
#include <buffer/Thing.h>
#include <io/file.h>

namespace sp {

template <typename Buffer>
static bool
do_dump(Buffer &sink, const dht::NodeId &id, dht::RoutingTable *t) noexcept {
  return bencode::e<Buffer>::dict(sink, [&id, t](Buffer &buffer) {
    if (!bencode::e<Buffer>::pair(buffer, "id", id.id, sizeof(id.id))) {
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
              return bencode::e<Buffer>::value(b, node.contact);
            });
            // return true;
          });
        });
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
dump(const dht::DHT &dht, const char *file) noexcept {
  sp::StaticCircularByteBuffer<128> b;
  // sp::StatucCircularByteBuffer<4096> b;

  fd f = fs::open_trunc(file);
  if (!f) {
    return false;
  }

  Sink s(b, &f, flush);

  if (!do_dump(s, dht.id, dht.root)) {
    return false;
  }

  if (!flush(s)) {
    return false;
  }

  return true;
}

//========================
template <typename Buffer, typename Bootstrap>
static bool
restore(Buffer &thing, /*OUT*/ dht::NodeId &id,
        /*OUT*/ Bootstrap &bs) noexcept {
  assertx(!is_marked(thing));

  return bencode::d<Buffer>::dict(thing, [&id, &bs](Buffer &buffer) {
    assertx(!is_marked(buffer));
    if (!bencode::d<Buffer>::pair(buffer, "id", id.id)) {
      return false;
    }

    assertx(!is_marked(buffer));
    if (!bencode::d<Buffer>::value(buffer, "nodes")) {
      return false;
    }

    assertx(!is_marked(buffer));
    return bencode::d<Buffer>::list(buffer, [&bs](Buffer &b) {
      assertx(!is_marked(b));
      return bencode::d<Buffer>::dict(b, [&bs](Buffer &buffer) {
        /**/
        Ipv4 ip(0);
        Port port(0);
        {
          assertx(!is_marked(buffer));
          if (!bencode::d<Buffer>::value(buffer, "ip")) {
            return false;
          }

          assertx(!is_marked(buffer));
          if (!bencode::d<Buffer>::value(buffer, ip)) {
            return false;
          }
          assertx(!is_marked(buffer));
        }
        {
          if (!bencode::d<Buffer>::value(buffer, "port")) {
            return false;
          }
          assertx(!is_marked(buffer));
          if (!bencode::d<Buffer>::value(buffer, port)) {
            return false;
          }
          assertx(!is_marked(buffer));
        }

        auto current = emplace(bs, ntohl(ip), ntohs(port));
        if (!current) {
          assertx(false);
          return false;
        }

        return true;
      });
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

  if (!restore(thing, dht.id, dht.bootstrap_contacts)) {
    return false;
  }

  return true;
}

} // namespace sp
