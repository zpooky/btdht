#include "dslbencode.h"

#include "dump.h"

#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>
#include <io/file.h>

namespace sp {

static bool
base(sp::Sink &sink, const dht::NodeId &id, dht::RoutingTable *t) noexcept {
  return true;
  // return bencode::e::dict(sink, [&id, t](sp::Buffer &buffer) {
  //   if (!bencode::e::pair(buffer, "ip", id.id, sizeof(id.id))) {
  //     return false;
  //   }
  //
  //   if (!bencode::e::value(buffer, "nodes")) {
  //     return false;
  //   }
  //
  //   if (!bencode::e::list(buffer, t, [](sp::Buffer &b, void *a) {
  //         auto *table = (dht::RoutingTable *)a;
  //         #<{(||)}>#
  //         return for_all(table, [&b](auto &current) {
  //           #<{(||)}>#
  //           return for_all(current.bucket, [&b](auto &node) {
  //             return bencode::e::value(b, node.contact);
  //           });
  //           // return true;
  //         });
  //       })) {
  //     return false;
  //   }
  //
  //   return true;
  // });
}
// TODO template Sink & Circularbuffer
// separate header and src
// manual template instant in header or src

static bool
flush(CircularByteBuffer &b, void *arg) noexcept {
  assert(arg);
  fd *f = (fd *)arg;

  if (!file::write(*f, b)) {
    return false;
  }

  return true;
}

bool
dump(sp::CircularByteBuffer &b, const dht::DHT &dht,
     const char *file) noexcept {

  reset(b);

  fd f = file::open_trunc(file);
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
