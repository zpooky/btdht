#include "db.h"

#include "Log.h"
#include "timeout.h"

#include <prng/util.h>

//TODO !! we never init array needle->peers it is always empty
namespace db {
//=====================================
dht::KeyValue *
lookup(dht::DHT &self, const dht::Infohash &infohash) noexcept {
  dht::KeyValue *needle;

  if ((needle = find(self.lookup_table, infohash))) {

    if (!is_empty(needle->peers)) {
      return needle;
    }
  }

  return nullptr;
} // db::lookup()

//=====================================
static void
peer_swap(dht::DHT &self, dht::Peer &f, dht::Peer &s) noexcept {
  dht::Peer *f_next = f.timeout_next, *f_priv = f.timeout_priv;
  dht::Peer *s_next = s.timeout_next, *s_priv = s.timeout_priv;

  timeout::unlink(self, &f);
  timeout::unlink(self, &s);

  using std::swap;
  swap(f, s);

  timeout::insert(f_priv, &s, f_next);
  timeout::insert(s_priv, &f, s_next);

  if (!self.timeout_peer) {
    self.timeout_peer = &f;
  }

  if (self.timeout_peer->activity > f.activity) {
    self.timeout_peer = &f;
  }

  if (self.timeout_peer->activity > s.activity) {
    self.timeout_peer = &s;
  }
}

bool
insert(dht::DHT &dht, const dht::Infohash &infohash, const Contact &contact,
       bool seed) noexcept {
  bool result = true;

  auto new_table = [&dht, infohash]() -> dht::KeyValue * {
    auto ires = insert(dht.lookup_table, infohash);
    return std::get<0>(ires);
  };

  auto add_peer = [&dht](dht::KeyValue &self, const Contact &c,
                         bool seed) -> dht::Peer * {
    dht::Peer p(c, dht.now, seed);
    if (!is_full(self.peers)) {
      sp::greater cmp;
      auto first = bin_find_gte(self.peers, p, cmp);
      dht::Peer *it = insert(self.peers, p);
      assertx(it);

      if (first) {
      // shift down
      Lit:
        auto priv = it - 1;
        peer_swap(dht, *it, *priv);
        it = priv;
        if (priv != first) {
          goto Lit;
        }
      }
      return it;
    }

    return nullptr;
  };

  auto find = [](dht::KeyValue &self, const Contact &s) -> dht::Peer * {
    sp::greater cmp;
    return bin_search(self.peers, s, cmp);
  };

  dht::KeyValue *table;
  if (!(table = lookup(dht, infohash))) {
    table = new_table();
  }

  if (table) {
    dht::Peer *existing;

    if ((existing = find(*table, contact))) {
      timeout::unlink(dht, existing);
      existing->activity = dht.now;
      timeout::append_all(dht, existing);
    } else {
      if ((existing = add_peer(*table, contact, seed))) {
        timeout::append_all(dht, existing);
        log::peer_db::insert(dht, infohash, contact);
      } else {
        result = false;
      }
    }

    if (is_empty(table->peers)) {
      // XXX if add false and create needle reclaim needle
    }
  }

  return result;
} // db::insert()

//=====================================
void
mint_token(dht::DHT &dht, dht::Node &id, Contact &, dht::Token &t) noexcept {
Lretry:
  prng::fill(dht.random, id.his_token.id);
  id.his_token.length = 5;

  if (!is_valid(id.his_token)) {
    goto Lretry;
  }

  t = id.his_token;
} // db::mint_token()

//=====================================
bool
valid(dht::DHT &, dht::Node &node, const dht::Token &token) noexcept {
  if (is_valid(token)) {
    return node.his_token == token;
  }

  return false;
} // db::valid()

//=====================================
Timeout
on_awake_peer_db(dht::DHT &self, sp::Buffer &) noexcept {
  dht::Config &config = self.config;
  Timeout result = self.now + config.peer_age_refresh;
  auto timeout = sp::Milliseconds(config.peer_age_refresh);

  dht::Peer *peer;
  while ((peer = timeout::take_peer(self, timeout))) {
    // TODO update dht::KeyValue->peers = peer
  }

  if ((peer = self.timeout_peer)) {
    result = peer->activity + config.peer_age_refresh;
  }

  return result;
}

//=====================================
} // namespace db
