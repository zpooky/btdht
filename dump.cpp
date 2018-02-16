#include "bencode.h"
#include "dump.h"

namespace sp {

static bool
base(sp::Buffer &b, dht::RoutingTable *t) {
  return bencode::e::dict(b, [](sp::Buffer &buffer) {
    /**/
    return true;
  });
}
bool
dump(const dht::DHT &, const char *) noexcept {
  return true;
}

} // namespace sp
