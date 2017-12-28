#ifndef SP_MAINLINE_DHT_MODUE_H
#define SP_MAINLINE_DHT_MODUE_H

#include "dht.h"
#include "shared.h"

//===========================================================
// Module
//===========================================================
namespace dht {

/*Module*/
struct Module {
  const char *query;

  bool (*response)(MessageContext &, void *) noexcept;
  void (*response_timeout)(DHT &, void *) noexcept;
  bool (*request)(MessageContext &) noexcept;

  Module() noexcept;
};

/*Modules*/
struct Modules {
  dht::Module module[24];
  std::size_t length;
  Timeout (*on_awake)(dht::DHT &, sp::Buffer &);

  Modules() noexcept;
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
