#include "db.h"

#include "Log.h"
#include "timeout.h"
#include <prng/util.h>

namespace db {

dht::KeyValue *
lookup(dht::DHT &dht, const dht::Infohash &infohash) noexcept {
  auto find_haystack = [](dht::KeyValue *current, const dht::Infohash &id) {
    // XXX tree?
  Lstart:
    if (current) {
      if (id == current->id) {
        return current;
      }
      current = current->next;
      goto Lstart;
    }

    return current;
  };

  auto is_expired = [&dht](dht::Peer &peer) {
    time_t peer_activity = peer.activity;

    // Determine to age if end of life is higher than now and make sure that
    // we have an internet connection by checking that we have received any
    // updates at all
    dht::Config config;
    time_t peer_eol = peer_activity + config.peer_age_refresh;
    if (peer_eol < dht.now) {
      if (dht.last_activity > peer_eol) {
        return true;
      }
    }
    return false;
  };

  auto reclaim = [&dht](dht::Peer *peer) { //
    // XXX pool
    delete peer;
  };

  auto reclaim_table = [&dht](dht::KeyValue *kv) { //
    // XXX pool
    delete kv;
  };

  dht::KeyValue *const needle = find_haystack(dht.lookup_table, infohash);
  if (needle) {
    dht::Peer dummy;
    dht::Peer *it = dummy.next = needle->peers;

    dht::Peer *previous = &dummy;
  Lloop:
    if (it) {
      dht::Peer *const next = it->next;
      if (is_expired(*it)) {
        reclaim(it);
        previous->next = next;
      } else {
        previous = it;
      }

      it = next;
      goto Lloop;
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
    auto result = new dht::KeyValue(infohash, dht.lookup_table);
    if (result) {
      dht.lookup_table = result;
    }
    return result;
  };

  auto add_peer = [&dht](dht::KeyValue &s, const Contact &c) {
    auto p = new dht::Peer(c, dht.now, s.peers);
    if (p) {
      s.peers = p;
      return true;
    }
    return false;
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
    dht::Peer *const existing = find(*table, contact);
    if (existing) {
      // TODO timeout::unlink_x(dht, existing);
      existing->activity = dht.now;
      timeout::append_all(dht, existing);

      return true;
    } else if (add_peer(*table, contact)) {
      log::peer_db::insert(dht, infohash, contact);

      return true;
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
