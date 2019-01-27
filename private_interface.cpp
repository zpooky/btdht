#include "Log.h"
#include "client.h"
#include "krpc.h"
#include "private_interface.h"
#include "search.h"
#include <util/assert.h>

//===========================================================
static void
search_dequeue(dht::DHT &dht, dht::Search *current) noexcept {
  assertx(current);
  dht::Search *next = current->next;
  dht::Search *priv = current->priv;

  if (dht.search_root == current) {
    assertx(!priv);
    dht.search_root = next;
  }

  if (next) {
    next->priv = priv;
  }

  if (priv) {
    priv->next = next;
  }

  current->next = nullptr;
  current->priv = nullptr;
}

static void
search_remove(dht::DHT &dht, dht::Search *current) noexcept {
  assertx(current);
  log::search::retire(dht, *current);
  search_dequeue(dht, current);
  bool res = remove(dht.searches, *current);
  assertx(res);
}

static void
search_enqueue(dht::DHT &dht, dht::Search *current) noexcept {
  assertx(current);
  assertx(!current->next);
  assertx(!current->priv);

  current->next = dht.search_root;
  if (current->next) {
    assertx(!current->next->priv);
    current->next->priv = current;
  }

  current->priv = nullptr;
  dht.search_root = current;
}

template <typename F>
static void
search_for_all(dht::DHT &dht, F f) noexcept {
  dht::Search *it = dht.search_root;
Lit:
  if (it) {
    dht::Search *const next = it->next;
    if (!f(it)) {
      return;
    }
    it = next;
    goto Lit;
  }
}

template <typename K, typename F>
K *
search_reduce(dht::DHT &dht, K *result, F f) noexcept {
  dht::Search *it = dht.search_root;
Lit:
  if (it) {
    dht::Search *const next = it->next;
    result = f(result, *it);
    it = next;
    goto Lit;
  }

  return result;
}

namespace interface_priv {
//===========================================================
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
scheduled_search(dht::DHT &dht, sp::Buffer &scratch) noexcept {
  auto result_f = [&](dht::Search *current) {
    while (!is_empty(current->result)) {
      reset(scratch);
      auto &search = current->search;
      std::size_t max(30);
      auto cx = sp::take(current->result, max);
      auto &remote = current->remote;

      auto res = client::priv::found(dht, scratch, search, remote, cx);
      if (res != client::Res::OK) {
        prepend(current->result, std::move(cx));
        break;
      }
    }

    if (dht.now > current->timeout) {
      if (is_empty(current->result)) {
        search_remove(dht, current);
      }
    }

    return true;
  };
  search_for_all(dht, result_f);

  /* TODO Move head from back so that tail nodes does get starved */
  search_for_all(dht, [&](dht::Search *s) {
    bool result = true;
    if (s->timeout > dht.now) {
    Lit:
      auto head = peek_head(s->queue);
      if (head) {
        reset(scratch);
        auto res =
            client::get_peers(dht, scratch, head->contact, s->search, s->ctx);
        if (res == client::Res::OK) {
          search_increment(s->ctx);
          drop_head(s->queue);
          goto Lit;
        }
        result = false;
      }
    }

    return result;
  });

  Timeout deftime(dht.now);
  Timeout *r = search_reduce(dht, &deftime, [](Timestamp *acum, auto &search) {
    if (search.timeout < *acum) {
      return &search.timeout;
    }

    return acum;
  });

  if (*r <= dht.now) {
    dht::Config &config = dht.config;
    /* XXX Arbitrary */
    return Timestamp(config.transaction_timeout);
  }

  return *r - dht.now;
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
  return krpc::response::dump(ctx.out, ctx.transaction, dht);
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
  dht::DHT &dht = ctx.dht;
  auto res = emplace(dht.searches, search, search);
  if (std::get<1>(res)) {
    dht::Search *const ins = std::get<0>(res);
    assertx(ins);
    ins->remote = ctx.remote;
    ins->timeout = dht.now + timeout;
    search_enqueue(dht, ins);

    /* Bootstrap search with content of routing table */
    for_all_node(dht.root, [&](const dht::Node &n) {
      insert_eager(ins->queue, dht::KContact(n, search.id));
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
handle_request(dht::MessageContext &ctx, const dht::Infohash &search) noexcept {
  auto &dht = ctx.dht;
  // XXX stop by searchid. if multiple receivers just remove sender as a
  // receiver

  dht::Search *const result = find(dht.searches, search);
  if (result) {
    search_remove(dht, result);
  }

  return krpc::response::search_stop(ctx.out, ctx.transaction);
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return krpc::d::request::search_stop(ctx, handle_request);
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_search_stop";
  module.request = on_request;
  module.response = nullptr;
}

//===========================================================
} // namespace search_stop
