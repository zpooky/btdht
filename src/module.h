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

  tx::TxHandle response;
  tx::TxCancelHandle response_timeout;
  bool (*request)(MessageContext &) noexcept;

  Module() noexcept;
};

struct ModulesAwake {
  using AwakeType = Timestamp (*)(DHT &, sp::Buffer &) noexcept;
  sp::StaticArray<AwakeType, 24> on_awake;
  ModulesAwake() noexcept;
};

// dht::Modules
struct Modules {
  ModulesAwake &awake;
  Module modules[24];
  std::size_t length;

  Modules(ModulesAwake &awake) noexcept;
};

Module &
module_for(Modules &, const char *key, Module &error) noexcept;

} // namespace dht

#endif
