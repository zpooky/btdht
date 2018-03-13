#ifndef SP_MAINLINE_DHT_MODULE_H
#define SP_MAINLINE_DHT_MODULE_H

#include "shared.h"
#include <collection/Array.h>

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
  using AwakeType = Timeout (*)(DHT &, sp::Buffer &) noexcept;
  sp::StaticArray<AwakeType, 4> on_awake;

  Modules() noexcept;
};

Module &
module_for(Modules &, const char *key, Module &error) noexcept;

} // namespace dht

#endif
