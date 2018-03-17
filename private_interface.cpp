#include "Log.h"
#include "client.h"
#include "krpc.h"
#include "private_interface.h"

namespace interface_priv {

static Timeout
scheduled_search(dht::DHT &, sp::Buffer &) noexcept;

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  dump::setup(modules.module[i++]);
  statistics::setup(modules.module[i++]);

  insert(modules.on_awake, scheduled_search);

  return true;
}

static Timeout
scheduled_search(dht::DHT &dht, sp::Buffer &b) noexcept {
  for_each(dht.searches, [&dht, &b](dht::Search &s) {
    /**/
  Lit:
    auto head = peek_head(s.queue);
    if (head) {
      reset(b);

      if (client::get_peers(dht, b, head->contact, s.search, s.ctx)) {
        ++s.ctx->ref_cnt;
      }
      drop_head(s.queue);
      goto Lit;
    }

    //
    for_all_node(dht.root, [&dht, &b, &s](const dht::Node &n) {
      if (!test(s.searched, n.id)) {
        reset(b);
        if (client::get_peers(dht, b, n.contact, s.search, s.ctx)) {
          ++s.ctx->ref_cnt;
          insert(s.searched, n.id);
        }
        return true;
      }
      return true;
    });
  });

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
handle_request(dht::MessageContext &ctx, const dht::Infohash &search) {
  auto &dht = ctx.dht;
  insert(dht.searches, search);
  return krpc::response::search(ctx.out, ctx.transaction);
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return krpc::d::request::search(ctx, handle_request);
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_search";
  module.request = on_request;
  module.response = nullptr;
}

} // namespace search
