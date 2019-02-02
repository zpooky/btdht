#ifndef SP_MAINLINE_DHT_DHT_INTERFACE_H
#define SP_MAINLINE_DHT_DHT_INTERFACE_H

#include "module.h"

namespace interface_dht {

bool
setup(dht::Modules &) noexcept;

} // namespace interface_dht

//===========================================================
// ping
//===========================================================
namespace ping {
void
setup(dht::Module &) noexcept;
} // namespace ping

//===========================================================
// find_node
//===========================================================
namespace find_node {
void
setup(dht::Module &) noexcept;
}

//===========================================================
// get_peers
//===========================================================
namespace get_peers {
void
setup(dht::Module &) noexcept;
}

//===========================================================
// announce_peer
//===========================================================
namespace announce_peer {
void
setup(dht::Module &) noexcept;
}

//===========================================================
// error
//===========================================================
namespace error {
void
setup(dht::Module &) noexcept;
}

#endif
