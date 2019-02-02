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

// dht::Modules
Modules::Modules() noexcept
    : module{}
    , length(0)
    , on_awake() {
}

Module &
module_for(Modules &ms, const char *key, Module &error) noexcept {
  for (std::size_t i = 0; i < ms.length; ++i) {
    dht::Module &current = ms.module[i];
    if (std::strcmp(current.query, key) == 0) {
      return current;
    }
  }
  return error;
}

} // namespace dht
