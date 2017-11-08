#ifndef SP_MAINLINE_DHT_MODUE_H
#define SP_MAINLINE_DHT_MODUE_H

#include "dht.h"
#include "shared.h"

//===========================================================
// Module
//===========================================================
namespace dht {
struct Module {
  const char *query;

  bool(*response) //
      (dht::DHT &, const dht::Peer &, bencode::d::Decoder &, sp::Buffer &);

  bool(*request) //
      (dht::DHT &, const dht::Peer &, bencode::d::Decoder &, sp::Buffer &);

  Module();
};

} // namespace dht

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
