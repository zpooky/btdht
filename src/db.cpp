#include "db.h"

#include "Log.h"
#include "timeout.h"
#include <hash/fnv.h>
#include <tree/bst_extra.h>

#include <prng/util.h>
namespace dht {
//=====================================
// dht::KeyValue
KeyValue::KeyValue(const dht::Infohash &pid) noexcept
    : id(pid)
    , peers{}
    , timeout_peer{nullptr}
    , name{nullptr} {
}

KeyValue::~KeyValue() {
  if (name) {
    free(name);
    name = nullptr;
  }
}

bool
operator>(const KeyValue &self, const dht::Infohash &o) noexcept {
  return self.id > o.id;
}

bool
operator>(const KeyValue &self, const KeyValue &o) noexcept {
  return self.id > o.id.id;
}

bool
operator>(const dht::Infohash &f, const KeyValue &s) noexcept {
  return f > s.id.id;
}

//=====================================
} // namespace dht

// TODO !! store peers using ip as key not ip:port
namespace db {
//=====================================
DHTMetaDatabase::DHTMetaDatabase(dht::Config &cfg, prng::xorshift32 &rnd,
                                 Timestamp &n, const dht::Options &opt)
    : scrape_client{n, opt.scrape_socket_path, opt.db_path}
    , lookup_table()
    , key{}
    , activity{0}
    , length_lookup_table{0}
    , last_generated{0}
    , random_samples{}
    , config{cfg}
    , random{rnd}
    , now{n} {
}
//}}}
//=====================================
dht::KeyValue *
lookup(DHTMetaDatabase &self, const dht::Infohash &infohash) noexcept {
  dht::KeyValue *needle;

  if ((needle = find(self.lookup_table, infohash))) {

    if (!is_empty(needle->peers)) {
      return needle;
    }
  }

  return nullptr;
} // db::lookup()

//=====================================
bool
insert(DHTMetaDatabase &self, const dht::Infohash &infohash,
       const Contact &contact, bool seed, const char *name) noexcept {
  auto new_table = [](DHTMetaDatabase &selfx, const dht::Infohash &ih) {
    auto ires = insert(selfx.lookup_table, ih);
    selfx.activity++;
    selfx.length_lookup_table++;
    return std::get<0>(ires);
  };

  dht::KeyValue *table;
  if (!(table = lookup(self, infohash))) {
    table = new_table(self, infohash);
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
      existing->activity = self.now;
      timeout::append_all(*table, existing);

      existing->seed = seed;
      existing->contact.port = contact.port;

      logger::peer_db::update(self, infohash, *existing);
    } else {
      existing = insert(table->peers, dht::Peer(contact, self.now, seed));
      spbt_scrape_client_send(self.scrape_client, infohash.id, contact);
      if (existing) {
        timeout::append_all(*table, existing);
        logger::peer_db::insert(self, infohash, contact);
      }
    }
  }

  return true;
} // db::insert()

//=====================================
static dht::TokenKey &
get_token_key(DHTMetaDatabase &self) noexcept {
  dht::Config &conf = self.config;
  if (self.now > (self.key[1].created + conf.token_key_refresh)) {
    self.key[0] = self.key[1];

    fill(self.random, &self.key[1].key, sizeof(self.key[1].key));
    self.key[1].created = self.now;
  }

  return self.key[1];
}

static dht::Token
create_token(dht::TokenKey &key, const Contact &remote) noexcept {
  dht::Token t;
  uint32_t h = fnv_1a::encode32(&key.key, sizeof(key.key));
  if (remote.ip.type == IpType::IPV4) {
    h = fnv_1a::encode(&remote.ip.ipv4, sizeof(remote.ip.ipv4), h);
  } else {
#ifdef IP_IPV6
    h = fnv_1a::encode(&remote.ip.ipv6, sizeof(remote.ip.ipv6), h);
#else
    assertx(false);
#endif
  }
  h = fnv_1a::encode(&remote.port, sizeof(remote.port), h);

  t.length = sizeof(h);
  memcpy(t.id, &h, t.length);

  return t;
}

void
mint_token(DHTMetaDatabase &self, const Contact &remote,
           dht::Token &t) noexcept {
  dht::TokenKey &key = get_token_key(self);
  t = create_token(key, remote);
}

bool
is_valid_token(DHTMetaDatabase &self, dht::Node &node,
               const dht::Token &token) noexcept {
  if (is_valid(token)) {
    dht::Token cmp = create_token(self.key[1], node.contact);
    if (cmp == token) {
      return true;
    }

    cmp = create_token(self.key[0], node.contact);
    return cmp == token;
  }

  return false;
} // db::is_valid_token()

//=====================================
Timestamp
on_awake_peer_db(DHTMetaDatabase &self, sp::Buffer &) noexcept {
  const sp::Milliseconds timeout(self.config.peer_age_refresh);
  sp::StaticArray<dht::KeyValue *, 16> empty;
  Timestamp next{self.now};

  binary::rec::inorder(self.lookup_table, [&](dht::KeyValue &cur) {
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
    assertx_n(remove(self.lookup_table, cur->id));
    self.length_lookup_table--;
    self.activity++;
  });

  Timestamp tmp(next + timeout);
  assertx(tmp > self.now);
  return tmp;
}

//=====================================
sp::UinStaticArray<dht::Infohash, 20> &
randomize_samples(DHTMetaDatabase &self) noexcept {
  if (!is_empty(self.lookup_table)) {
    sp::Timestamp next_generated =
        self.last_generated + self.config.db_samples_refresh_interval;
    if (next_generated <= self.now) {
      clear(self.random_samples);
      binary::rec::inorder(self.lookup_table, [&](dht::KeyValue &cur) {
        if (!is_full(self.random_samples)) {
          insert(self.random_samples, dht::Infohash(cur.id));
        } else {
          auto idx = random(self.random) % self.length_lookup_table;
          if (idx < capacity(self.random_samples)) {
            insert_at(self.random_samples, idx, dht::Infohash(cur.id));
          }
        }
      });
      self.last_generated = self.now;
    }
  }
  return self.random_samples;
}

//=====================================
std::uint32_t
next_randomize_samples(DHTMetaDatabase &self, const Timestamp &now) noexcept {
  Timestamp next_refresh =
      self.last_generated + self.config.db_samples_refresh_interval;
  if (next_refresh <= now) {
    return (uint32_t)sp::Seconds(self.config.db_samples_refresh_interval).value;
  }
  Timestamp delta = next_refresh - now;
  return (uint32_t)sp::Seconds(delta).value;
}

//=====================================
} // namespace db
