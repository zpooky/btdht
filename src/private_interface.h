#ifndef SP_MAINLINE_DHT_PRIVATE_INTERFACE_H
#define SP_MAINLINE_DHT_PRIVATE_INTERFACE_H

#include "module.h"

namespace interface_priv {
//===========================================================
bool
setup(dht::Modules &) noexcept;

//===========================================================
} // namespace interface_priv

//===========================================================
// dump
//===========================================================
namespace dump {
void
setup(dht::Module &) noexcept;
} // namespace dump

//===========================================================
// statistics
//===========================================================
namespace statistics {
void
setup(dht::Module &) noexcept;
} // namespace statistics

//===========================================================
// search
//===========================================================
namespace search {
void
setup(dht::Module &) noexcept;
} // namespace search

//===========================================================
// search stop
//===========================================================
namespace search_stop {
void
setup(dht::Module &) noexcept;
} // namespace search_stop

#endif
