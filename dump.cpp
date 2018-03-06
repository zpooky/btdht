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

// TODO template Sink & Circularbuffer
// separate header and src
// manual template instant in header or src

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
dump(sp::CircularByteBuffer &b, const dht::DHT &dht,
     const char *file) noexcept {

  reset(b);

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

} // namespace sp
