#ifndef SP_MAINLINE_DHT_DB_H
#define SP_MAINLINE_DHT_DB_H

// #include "shared.h"
#include "spbt_scrape_client.h"
#include "util.h"
#include "Options.h"

#include <collection/Array.h>
#include <list/SkipList.h>
#include <tree/avl.h>

namespace dht {
//=====================================
struct KeyValue {
  dht::Infohash id;
  sp::SkipList<dht::Peer, 6> peers;
  dht::Peer *timeout_peer;
  char *name;

  explicit KeyValue(const dht::Infohash &) noexcept;

  KeyValue(const KeyValue &) = delete;
  KeyValue(const KeyValue &&) = delete;

  KeyValue &
  operator=(const KeyValue &) = delete;
  KeyValue &
  operator=(const KeyValue &&) = delete;

  ~KeyValue();
};

bool
operator>(const KeyValue &, const dht::Infohash &) noexcept;

bool
operator>(const KeyValue &, const KeyValue &) noexcept;

bool
operator>(const dht::Infohash &, const KeyValue &) noexcept;
} // namespace dht

namespace db {

//=====================================
struct DHTMetaDatabase {
  dht::DHTMeta_spbt_scrape_client scrape_client;
  avl::Tree<dht::KeyValue> lookup_table;
  dht::TokenKey key[2];
  uint32_t activity;
  uint32_t length_lookup_table;
  Timestamp last_generated{0};
  sp::UinStaticArray<dht::Infohash, 20> random_samples;

  dht::Config &config;
  prng::xorshift32 &random;
  Timestamp &now;

  DHTMetaDatabase(dht::Config &, prng::xorshift32 &, Timestamp &,
                  const dht::Options &);

  ~DHTMetaDatabase() {
  }
};

//=====================================
dht::KeyValue *
lookup(DHTMetaDatabase &, const dht::Infohash &key) noexcept;

//=====================================
bool
insert(DHTMetaDatabase &, const dht::Infohash &key, const Contact &value,
       bool seed, const char *name) noexcept;

//=====================================
void
mint_token(DHTMetaDatabase &, const Contact &, dht::Token &) noexcept;

bool
is_valid_token(DHTMetaDatabase &, dht::Node &, const dht::Token &) noexcept;

//=====================================
Timestamp
on_awake_peer_db(DHTMetaDatabase &, sp::Buffer &) noexcept;

//=====================================
sp::UinStaticArray<dht::Infohash, 20> &
randomize_samples(DHTMetaDatabase &) noexcept;

//=====================================
std::uint32_t
next_randomize_samples(DHTMetaDatabase &, const Timestamp &now) noexcept;

//=====================================

} // namespace db

#endif
