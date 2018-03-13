#include "priv_bencode.h"

#include "dump.h"

#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>
#include <io/file.h>

namespace sp {

template <typename Buffer>
static bool
base(Buffer &sink, const dht::NodeId &id, dht::RoutingTable *t) noexcept {
  return bencode<Buffer>::e::dict(sink, [&id, t](Buffer &buffer) {
    if (!bencode<Buffer>::e::pair(buffer, "ip", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode<Buffer>::e::value(buffer, "nodes")) {
      return false;
    }

    if (!bencode<Buffer>::e::list(buffer, t, [](Buffer &b, void *a) {
          auto *table = (dht::RoutingTable *)a;
          /**/
          return for_all(table, [&b](auto &current) {
            /**/
            return for_all(current.bucket, [&b](auto &node) {
              return bencode<Buffer>::e::value(b, node.contact);
            });
            // return true;
          });
        })) {
      return false;
    }

    return true;
  });
}

static bool
flush(CircularByteBuffer &b, void *arg) noexcept {
  assert(arg);
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

  if (!base(s, dht.id, dht.root)) {
    return false;
  }

  if (!flush(s)) {
    return false;
  }

  return true;
}

bool
restore(dht::DHT &, const char *file) noexcept {
  fd f = fs::open_read(file);
  if (!f) {
    return false;
  }
  sp::StaticCircularByteBuffer<64> b;

  // TODO

  return true;
}

} // namespace sp
