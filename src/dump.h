#ifndef SP_MAINLINE_DHT_DUMP_H
#define SP_MAINLINE_DHT_DUMP_H

#include "shared.h"

// TODO dump should be a raw kind of image of the DHT struct
namespace sp {
//========================
bool
dump(const dht::DHT &, const char *path) noexcept;

//========================
bool
restore(dht::DHT &, const char *path) noexcept;

//========================
} // namespace sp

#endif
