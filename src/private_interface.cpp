#include "private_interface.h"
#include "Log.h"
#include "client.h"
#include "krpc.h"
#include "search.h"
#include <util/assert.h>

//===========================================================
// dump
//===========================================================
namespace dump {
void
setup(dht::Module &) noexcept;
} // namespace dump

//===========================================================
// statistics
//===========================================================
namespace statistics {
void
setup(dht::Module &) noexcept;
} // namespace statistics

//===========================================================
// search
//===========================================================
namespace search {
void
setup(dht::Module &) noexcept;
} // namespace search

//===========================================================
// search stop
//===========================================================
namespace search_stop {
void
setup(dht::Module &) noexcept;
} // namespace search_stop

//===========================================================
// Announce
//===========================================================
namespace announce_this {
void
setup(dht::Module &) noexcept;
} // namespace announce_this

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
  logger::search::retire(dht, *current);
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
static K *
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
static Timestamp
scheduled_search(dht::DHT &, sp::Buffer &) noexcept;

bool
setup(dht::Modules &modules) noexcept {
  std::size_t &i = modules.length;
  dump::setup(modules.modules[i++]);
  statistics::setup(modules.modules[i++]);
  search::setup(modules.modules[i++]);
  search_stop::setup(modules.modules[i++]);

  insert(modules.on_awake, scheduled_search);

  return true;
}

static Timestamp
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
        ++current->fail;
        prepend(current->result, std::move(cx));
        break;
      } else {
        current->fail = 0;
      }
    }

    if (dht.now > current->timeout) {
      if (is_empty(current->result) || current->fail > 2) {
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

  Timestamp deftime(dht.now);
  Timestamp *next;
  next = search_reduce(dht, &deftime, [](Timestamp *it, dht::Search &search) {
    if (search.timeout < *it) {
      return &search.timeout;
    }

    return it;
  });

  if (*next <= dht.now) {
    dht::Config &cfg = dht.config;
    /* XXX Arbitrary */
    return dht.now + cfg.transaction_timeout;
  }

  assertxs(*next > dht.now, uint64_t(*next), uint64_t(dht.now));
  return *next;
}

} // namespace interface_priv

//===========================================================
// dump
//===========================================================
namespace dump {
static bool
on_request(dht::MessageContext &ctx) noexcept {
  logger::receive::req::dump(ctx);

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
      insert_eager(ins->queue, dht::KContact(n.id.id, n.contact, search.id));
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
// Announce
//===========================================================
namespace announce_this {
static bool
handle_request(dht::MessageContext &ctx, const dht::Infohash &,
               const sp::UinStaticArray<Contact, 256> &) noexcept {
  // auto &dht = ctx.dht;
  // TODO

  return krpc::response::announce_this(ctx.out, ctx.transaction);
}

static bool
on_request(dht::MessageContext &ctx) noexcept {
  return krpc::d::request::announce_this(ctx, handle_request);
}

void
setup(dht::Module &module) noexcept {
  module.query = "sp_announce";
  module.request = on_request;
  module.response = nullptr;
}
} // namespace announce_this

//===========================================================
} // namespace search_stop
