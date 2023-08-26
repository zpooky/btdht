#include "db.h"

#include "Log.h"
#include "timeout.h"
#include <hash/fnv.h>
#include <tree/bst_extra.h>

#include <prng/util.h>

// TODO !! store peers using ip as key not ip:port
namespace db {
//=====================================
dht::KeyValue *
lookup(dht::DHT &self, const dht::Infohash &infohash) noexcept {
  dht::KeyValue *needle;

  if ((needle = find(self.db.lookup_table, infohash))) {

    if (!is_empty(needle->peers)) {
      return needle;
    }
  }

  return nullptr;
} // db::lookup()

//=====================================
bool
insert(dht::DHT &dht, const dht::Infohash &infohash, const Contact &contact,
       bool seed, const char *name) noexcept {
  auto new_table = [](dht::DHT &self, const dht::Infohash &ih) {
    auto ires = insert(self.db.lookup_table, ih);
    self.db.activity++;
    self.db.length_lookup_table++;
    return std::get<0>(ires);
  };

  dht::KeyValue *table;
  if (!(table = lookup(dht, infohash))) {
    table = new_table(dht, infohash);
  }

  if (table) {
    if (strlen(name) > 0) {
      if (!table->name) {
        table->name = (char *)calloc(128 + 1, sizeof(char));
      }
      strncpy(table->name, name, 128);
    }

    dht::Peer *existing;

    if ((existing = find(table->peers, contact))) {
      timeout::unlink(*table, existing);
      existing->activity = dht.now;
      timeout::append_all(*table, existing);

      existing->seed = seed;
      existing->contact.port = contact.port;

      logger::peer_db::update(dht, infohash, *existing);
    } else {
      existing = insert(table->peers, dht::Peer(contact, dht.now, seed));
      if (existing) {
        timeout::append_all(*table, existing);
        logger::peer_db::insert(dht, infohash, contact);
      }
    }
  }

  return true;
} // db::insert()

//=====================================
static dht::TokenKey &
get_token_key(dht::DHT &self) noexcept {
  dht::Config &conf = self.config;
  if (self.now > (self.db.key[1].created + conf.token_key_refresh)) {
    self.db.key[0] = self.db.key[1];

    fill(self.random, &self.db.key[1].key, sizeof(self.db.key[1].key));
    self.db.key[1].created = self.now;
  }

  return self.db.key[1];
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
is_valid_token(dht::DHT &self, dht::Node &node,
               const dht::Token &token) noexcept {
  if (is_valid(token)) {
    dht::Token cmp = ee(self.db.key[1], node.contact);
    if (cmp == token) {
      return true;
    }

    cmp = ee(self.db.key[0], node.contact);
    return cmp == token;
  }

  return false;
} // db::valid()

//=====================================
Timestamp
on_awake_peer_db(dht::DHT &self, sp::Buffer &) noexcept {
  const sp::Milliseconds timeout(self.config.peer_age_refresh);
  sp::StaticArray<dht::KeyValue *, 16> empty;
  Timestamp next{self.now};

  binary::rec::inorder(self.db.lookup_table, [&](dht::KeyValue &cur) {
    dht::Peer *peer;
    while ((peer = timeout::take_peer(self, cur, timeout))) {
      assertx_n(remove(cur.peers, *peer));
    }

    if (cur.timeout_peer) {
      next = std::min(next, cur.timeout_peer->activity);
    }
    if (is_empty(cur.peers)) {
      assertx(!cur.timeout_peer);
      insert(empty, &cur);
    }
  });

  for_each(empty, [&](auto cur) { //
    assertx_n(remove(self.db.lookup_table, cur->id));
    self.db.length_lookup_table--;
    self.db.activity++;
  });

  assertx((next + timeout) > self.now);
  return next + timeout;
}

//=====================================
sp::UinStaticArray<dht::Infohash, 20> &
randomize_samples(dht::DHT &self) noexcept {
  if (!is_empty(self.db.lookup_table)) {
    sp::Timestamp next_generated =
        self.db.last_generated + self.config.db_samples_refresh_interval;
    if (next_generated <= self.now) {
      clear(self.db.random_samples);
      binary::rec::inorder(self.db.lookup_table, [&](dht::KeyValue &cur) {
        if (!is_full(self.db.random_samples)) {
          insert(self.db.random_samples, dht::Infohash(cur.id));
        } else {
          auto idx = random(self.random) % self.db.length_lookup_table;
          if (idx < capacity(self.db.random_samples)) {
            insert_at(self.db.random_samples, idx, dht::Infohash(cur.id));
          }
        }
      });
      self.db.last_generated = self.now;
    }
  }
  return self.db.random_samples;
}

//=====================================
std::uint32_t
next_randomize_samples(const dht::DHT &self) noexcept {
  Timestamp next_refresh =
      self.db.last_generated + self.config.db_samples_refresh_interval;
  if (next_refresh <= self.now) {
    return (uint32_t)sp::Seconds(self.config.db_samples_refresh_interval).value;
  }
  Timestamp delta = next_refresh - self.now;
  return (uint32_t)sp::Seconds(delta).value;
}

//=====================================
} // namespace db
