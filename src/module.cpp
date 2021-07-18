#include "module.h"
#include <cstring>

//===========================================================
// Module
//===========================================================
namespace dht {

// dht::Module
Module::Module() noexcept
    : query(nullptr)
    , response(nullptr)
    , response_timeout(nullptr)
    , request(nullptr) {
}

ModulesAwake::ModulesAwake() noexcept
    : on_awake() {
}

// dht::Modules
Modules::Modules(ModulesAwake &_awake) noexcept
    : awake{_awake}
    , modules{}
    , length(0) {
}

Module &
module_for(Modules &ms, const char *key, Module &error) noexcept {
  for (std::size_t i = 0; i < ms.length; ++i) {
    dht::Module &current = ms.modules[i];
    if (std::strcmp(current.query, key) == 0) {
      return current;
    }
  }
  return error;
}

} // namespace dht
