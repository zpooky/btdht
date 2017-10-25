#ifndef SP_MAINLINE_DHT_MAINLINE_DHT_H
#define SP_MAINLINE_DHT_MAINLINE_DHT_H

#include "kadmelia.h"

namespace dht {
/*mainline dht*/
using infohash = kadmelia::Key;

void
get_peers(const infohash &);
} // namespace dht
#endif
