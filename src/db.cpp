#include "db.h"

#include "Log.h"
#include "timeout.h"
#include <hash/fnv.h>
#include <tree/bst_extra.h>

#include <prng/util.h>

// TODO !! we never init array needle->peers it is always empty
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
#if 0
static void
peer_swap(dht::KeyValue &self, dht::Peer &f, dht::Peer &s) noexcept {
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
#endif

bool
insert(dht::DHT &dht, const dht::Infohash &infohash, const Contact &contact,
       bool seed) noexcept {
  auto new_table = [](dht::DHT &self, const dht::Infohash &ih) {
    auto ires = insert(self.lookup_table, ih);
    return std::get<0>(ires);
  };

#if 0
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
#endif

  dht::KeyValue *table;
  if (!(table = lookup(dht, infohash))) {
    table = new_table(dht, infohash);
  }

  if (table) {
    dht::Peer *existing;

    if ((existing = find(table->peers, contact))) {
      timeout::unlink(*table, existing);
      existing->activity = dht.now;
      timeout::append_all(*table, existing);
      existing->seed = seed;
      log::peer_db::update(dht, infohash, *existing);
    } else {
      existing = insert(table->peers, dht::Peer(contact, dht.now, seed));
      if (existing) {
        timeout::append_all(*table, existing);
        log::peer_db::insert(dht, infohash, contact);
      }
    }
  }

  return true;
} // db::insert()

//=====================================
static dht::TokenKey &
get_token_key(dht::DHT &self) noexcept {
  dht::Config &conf = self.config;
  if (self.now > (self.key[1].created + conf.token_key_refresh)) {
    self.key[0] = self.key[1];

    fill(self.random, &self.key[1].key, sizeof(self.key[1].key));
    self.key[1].created = self.now;
  }

  return self.key[1];
}

static dht::Token
ee(dht::TokenKey &key, const Contact &remote) noexcept {
  dht::Token t;
  uint32_t hash = fnv_1a::encode32(&key.key, sizeof(key.key));
  if (remote.ip.type == IpType::IPV4) {
    hash = fnv_1a::encode(&remote.ip.ipv4, sizeof(remote.ip.ipv4), hash);
  } else {
    hash = fnv_1a::encode(&remote.ip.ipv6, sizeof(remote.ip.ipv6), hash);
  }
  hash = fnv_1a::encode(&remote.port, sizeof(remote.port), hash);

  t.length = sizeof(hash);
  memcpy(t.id, &hash, t.length);

  return t;
}

void
mint_token(dht::DHT &self, const Contact &remote, dht::Token &t) noexcept {
  dht::TokenKey &key = get_token_key(self);
  t = ee(key, remote);
}

//=====================================
bool
valid(dht::DHT &self, dht::Node &node, const dht::Token &token) noexcept {
  if (is_valid(token)) {
    dht::Token cmp = ee(self.key[1], node.contact);
    if (cmp == token) {
      return true;
    }

    cmp = ee(self.key[0], node.contact);
    return cmp == token;
  }

  return false;
} // db::valid()

//=====================================
Timeout
on_awake_peer_db(dht::DHT &self, sp::Buffer &) noexcept {
  sp::Milliseconds timeout(self.config.peer_age_refresh);
  sp::StaticArray<dht::KeyValue *, 16> empty;
  Timestamp result{self.now};

  binary::rec::inorder(self.lookup_table, [&](dht::KeyValue &cur) {
    dht::Peer *peer;
    while ((peer = timeout::take_peer(self, cur, timeout))) {
      assertx_n(remove(cur.peers, *peer));
    }

    if (cur.timeout_peer) {
      result = std::min(result, cur.timeout_peer->activity);
    }
    if (is_empty(cur.peers)) {
      insert(empty, &cur);
    }
  });

  for_each(empty, [&](auto cur) { //
    assertx_n(remove(self.lookup_table, cur->id));
  });

  result = self.now - result;
  assertx(Timeout(self.config.peer_age_refresh) >= result);
  return Timeout(self.config.peer_age_refresh) - result;
}

//=====================================
} // namespace db
