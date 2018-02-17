#include "dslbencode.h"
#include "dump.h"

namespace sp {

static bool
base(sp::Buffer &buf, const dht::NodeId &id, dht::RoutingTable *t) {
  return bencode::e::dict(buf, [&id, t](sp::Buffer &buffer) {
    if (!bencode::e::pair(buffer, "ip", id.id, sizeof(id.id))) {
      return false;
    }

    if (!bencode::e::value(buffer, "nodes")) {
      return false;
    }

    if (!bencode::e::list(buffer, t, [](sp::Buffer &b, void *a) {
          auto *table = (dht::RoutingTable *)a;
          /**/
          return for_all(table, [&b](auto &current) {
            /**/
            return for_all(current.bucket, [&b](auto &node) {
              return bencode::e::value(b, node.contact);
            });
            // return true;
          });
        })) {
      return false;
    }

    return true;
  });
}
bool
dump(sp::Buffer &b, const dht::DHT &dht, const file::Path &file) noexcept {
  reset(b);
  if (!base(b, dht.id, dht.root)) {
    return false;
  }
  flip(b);

  fd f = file::open_trunc(file);
  if (!f) {
    return false;
  }

  if (!file::write(f, b)) {
    return false;
  }

  return true;
}

} // namespace sp
