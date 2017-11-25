#ifndef SP_MAINLINE_DHT_MODUE_H
#define SP_MAINLINE_DHT_MODUE_H

#include "dht.h"
#include "shared.h"

//===========================================================
// Module
//===========================================================
namespace dht {

/*MessageContext*/
struct MessageContext {
  const char *query;
  DHT &dht;
  bencode::d::Decoder &in;
  sp::Buffer &out;
  const krpc::Transaction &transaction;
  Contact remote;
  const time_t now;
  MessageContext(const char *, DHT &, bencode::d::Decoder &, sp::Buffer &,
                 const krpc::Transaction &, Contact, time_t) noexcept;
};

/*Module*/
struct Module {
  const char *query;

  bool (*response)(MessageContext &);
  bool (*request)(MessageContext &);

  Module() noexcept;
};

/*Modules*/
struct Modules {
  dht::Module module[24];
  std::size_t length;
  Timeout (*on_awake)(dht::DHT &, fd &, sp::Buffer &, time_t);

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
