#include "Log.h"
#include "client.h"
#include "krpc.h"
#include "private_interface.h"
#include <util/assert.h>

namespace interface_priv {

static Timeout
scheduled_search(dht::DHT &, sp::Buffer &) noexcept;

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  dump::setup(modules.module[i++]);
  statistics::setup(modules.module[i++]);
  search::setup(modules.module[i++]);
  search_stop::setup(modules.module[i++]);

  insert(modules.on_awake, scheduled_search);

  return true;
}

static Timeout
scheduled_search(dht::DHT &dht, sp::Buffer &scrtch) noexcept {
  remove_if(dht.searches, [&](dht::Search &current) {
    if (!is_empty(current.result)) {
      reset(scrtch);
      auto &search = current.search;
      auto cx = sp::take(current.result, std::size_t(30));
      auto res = client::priv::found(dht, scrtch, search, current.remote, cx);
      if (res != client::Res::OK) {
        prepend(current.result, std::move(cx));
      }
    }

    bool res = dht.now >= current.timeout;
    if (res) {
      log::search::retire(dht, current);
    }

    return res;
  });

  for_each(dht.searches, [&dht, &scrtch](dht::Search &s) {
    /**/
  Lit:
    auto head = peek_head(s.queue);
    if (head) {

      reset(scrtch);
      auto res = client::get_peers(dht, scrtch, head->contact, s.search, s.ctx);
      if (res == client::Res::OK) {
        ++s.ctx->ref_cnt;
        drop_head(s.queue);
        goto Lit;
      }
    }

    //
  });

  dht::Config config;
  Timeout result(config.refresh_interval);
  result = reduce(dht.searches, result, [](auto acum, auto &search) {
    if (search.timeout < acum) {
      return search.timeout;
    }

    return acum;
  });
  if (!is_empty(dht.searches)) {
    result = std::min(result, Timeout(config.transaction_timeout));
  }

  return Timeout(result);
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
  assertx(r);

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
handle_request(dht::MessageContext &ctx, const dht::Infohash &search,
               sp::Seconds timeout) {
  auto &dht = ctx.dht;

  dht::Search *ins = emplace(dht.searches, search, ctx.remote);
  assertx(ins);

  if (ins) {
    ins->timeout = dht.now + timeout;

    for_all_node(dht.root, [&dht, &search, &ins](const dht::Node &n) {
      insert_eager(ins->queue, dht::K(n, search.id));
      return true;
    });
  }

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

//===========================================================
// search stop
//===========================================================
namespace search_stop {

static bool
on_request(dht::MessageContext &) noexcept {
  // TODO
  return true;
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_search_stop";
  module.request = on_request;
  module.response = nullptr;
}
} // namespace search_stop
