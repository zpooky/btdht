#ifndef SP_MAINLINE_DHT_MODULE_H
#define SP_MAINLINE_DHT_MODULE_H

#include "shared.h"

//===========================================================
// Module
//===========================================================
namespace dht {

// dht::Module
struct Module {
  const char *query;

  bool (*response)(MessageContext &, void *) noexcept;
  void (*response_timeout)(DHT &, void *) noexcept;
  bool (*request)(MessageContext &) noexcept;

  Module() noexcept;
};

// dht::Modules
struct Modules {
  Module module[24];
  std::size_t length;
  Timeout (*on_awake)(DHT &, sp::Buffer &) noexcept;

  Modules() noexcept;
};

Module &
module_for(Modules &, const char *key, Module &error) noexcept;

} // namespace dht

#endif
