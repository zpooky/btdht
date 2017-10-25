#include "mainline_dht.h"
namespace dht {
void
get_peers(const infohash &key) {
  kadmelia::FIND_VALUE(key);
}

} // namespace dht
