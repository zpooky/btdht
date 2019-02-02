#include "db.h"

#include "Log.h"
#include "timeout.h"
#include <prng/util.h>

namespace db {

dht::KeyValue *
lookup(dht::DHT &self, const dht::Infohash &infohash) noexcept {
  auto find_haystack = [&self](const dht::Infohash &id) -> dht::KeyValue * {
#if 0
    dht::KeyValue *const start = self.lookup_table;
    dht::KeyValue *current = start;
    // XXX tree?
    while (current) {
      if (id == current->id) {
        return current;
      }

      current = current->next;
      if (current == start) {
        break;
      }
    }
#endif
    return find(self.lookup_table, id);
  };

  auto is_expired = [&self](dht::Peer &peer) {
    Timestamp peer_activity = peer.activity;

    // Determine to age if end of life is higher than now and make sure that
    // we have an internet connection by checking that we have received any
    // updates at all
    dht::Config config;
    Timestamp peer_eol = peer_activity + config.peer_age_refresh;
    if (peer_eol < self.now) {
      if (self.last_activity > peer_eol) {
        return true;
      }
    }
    return false;
  };

  auto reclaim = [&self](dht::Peer *peer) { //
    assertx(peer);
    timeout::unlink(self, peer);
    // XXX pool
    delete peer;
  };

  auto reclaim_table = [&self](dht::KeyValue *kv) { //
    bool res = remove(self.lookup_table, *kv);
    assertx(res);
    // XXX pool
  };

  dht::KeyValue *const needle = find_haystack(infohash);
  if (needle) {
    dht::Peer dummy;
    dht::Peer *it = dummy.next = needle->peers;

    dht::Peer *previous = &dummy;
    while (it) {
      dht::Peer *const next = it->next;
      if (is_expired(*it)) {
        reclaim(it);
        previous->next = next;
      } else {
        previous = it;
      }

      it = next;
    }

    needle->peers = dummy.next;
    if (needle->peers) {
      return needle;
    }

    reclaim_table(needle);
  }

  return nullptr;
} // db::lookup()

bool
insert(dht::DHT &dht, const dht::Infohash &infohash,
       const Contact &contact) noexcept {

  auto new_table = [&dht, infohash]() -> dht::KeyValue * {
    auto result = insert(dht.lookup_table, infohash);
    return std::get<0>(result);
  };

  auto add_peer = [&dht](dht::KeyValue &s, const Contact &c) {
    s.peers = new dht::Peer(c, dht.now, s.peers);
    return s.peers;
  };

  auto find = [](dht::KeyValue &t, const Contact &s) {
    dht::Peer *it = t.peers;
  Lstart:
    if (it) {
      if (it->contact == s) {
        return it;
      }
      it = it->next;
      goto Lstart;
    }

    return (dht::Peer *)nullptr;
  };

  dht::KeyValue *table = lookup(dht, infohash);
  if (!table) {
    table = new_table();
  }

  if (table) {
    dht::Peer *existing = find(*table, contact);
    if (existing) {
      timeout::unlink(dht, existing);
      existing->activity = dht.now;
      timeout::append_all(dht, existing);

      return true;
    } else {
      existing = add_peer(*table, contact);
      if (existing) {
        timeout::append_all(dht, existing);
        log::peer_db::insert(dht, infohash, contact);
        return true;
      }
    }
    if (!table->peers) {
      // TODO if add false and create needle reclaim needle
    }
  }

  return false;
} // db::insert()

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

bool
valid(dht::DHT &, dht::Node &node, const dht::Token &token) noexcept {
  if (is_valid(token)) {
    return node.his_token == token;
  }
  return false;
} // db::valid()

} // namespace db
