#include "Log.h"
#include "krpc.h"
#include "private_interface.h"

namespace interface_priv {

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  dump::setup(modules.module[i++]);
  statistics::setup(modules.module[i++]);

  return true;
}

} // namespace interface_priv

//===========================================================
// dump
//===========================================================
namespace dump {

static bool
on_request(dht::MessageContext &ctx) noexcept {
  log::receive::req::dump(ctx);

  dht::DHT &dht = ctx.dht;
  krpc::response::dump(ctx.out, ctx.transaction, dht);
  return true;
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_dump";
  module.request = on_request;
  module.response = nullptr;
}

} // namespace dump

//===========================================================
// statistics
//===========================================================
namespace statistics {

static bool
on_request(dht::MessageContext &ctx) noexcept {
  // TODO
  return true;
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_statistics";
  module.request = on_request;
  module.response = nullptr;
}

} // namespace statistics
