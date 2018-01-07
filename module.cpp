#include "module.h"

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
    , on_awake(nullptr) {
}

} // namespace dht
