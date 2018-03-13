#include "Log.h"
#include "krpc.h"
#include "private_interface.h"

namespace interface_priv {

static Timeout
on_awake(dht::DHT &, sp::Buffer &) noexcept;

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  dump::setup(modules.module[i++]);
  statistics::setup(modules.module[i++]);

  insert(modules.on_awake, on_awake);

  return true;
}

static Timeout
on_awake(dht::DHT &, sp::Buffer &) noexcept {
  dht::Config config;
  Timeout next(config.refresh_interval);
  return next;
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
  bool r = krpc::response::dump(ctx.out, ctx.transaction, dht);
  assert(r);

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
  dht::DHT &dht = ctx.dht;
  return krpc::response::statistics(ctx.out, ctx.transaction, dht.statistics);
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_statistics";
  module.request = on_request;
  module.response = nullptr;
}

} // namespace statistics

//===========================================================
// search
//===========================================================
namespace search {

static bool
on_request(dht::MessageContext &) noexcept {
  // TODO
  return true;
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_search";
  module.request = on_request;
  module.response = nullptr;
}

} // namespace search
