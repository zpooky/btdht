#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

#include "ip_election.h"
#include "util.h"
#include <cstdio> //debug
#include <cstdlib>

#include "bencode.h"
#include "dstack.h"
#include <hash/util.h>
#include <list/FixedList.h>
#include <list/LinkedList.h>
#include <tree/StaticTree.h>

#include <core.h>
#include <heap/binary.h>
#include <io/fd.h>
#include <list/SkipList.h>
#include <prng/xorshift.h>
#include <tree/avl.h>
#include <util/maybe.h>

namespace dht {
struct MessageContext;
struct DHT;
enum class Domain { Domain_public, Domain_private };
} // namespace dht

//=====================================
namespace krpc {
// krpc::ParseContext
struct ParseContext {
  dht::Domain domain;
  dht::DHT &ctx;

  sp::Buffer &decoder;
  Transaction tx;

  char msg_type[16];
  char query[64];
  sp::byte remote_version[16];
  bool read_only = false;

  sp::maybe<Contact> ip_vote;

  ParseContext(dht::Domain, dht::DHT &ctx, sp::Buffer &) noexcept;

  ParseContext(ParseContext &, sp::Buffer &) noexcept;

  ParseContext(const ParseContext &) = delete;
  ParseContext(const ParseContext &&) = delete;

  ParseContext &
  operator=(const ParseContext &) = delete;
  ParseContext &
  operator=(const ParseContext &&) = delete;
};

} // namespace krpc

//=====================================
namespace tx {
struct Tx;

using TxCancelHandle = void (*)(dht::DHT &, const krpc::Transaction &,
                                Timestamp, void *);
using TxHandle = bool (*)(dht::MessageContext &, void *);
// dht::TxContext
struct TxContext {
  TxHandle int_handle;
  TxCancelHandle int_cancel;
  void *closure;

  TxContext(TxHandle, TxCancelHandle, void *) noexcept;
  TxContext() noexcept;

  // TxContext(const TxContext &) noexcept;
  // TxContext(const TxContext &&) noexcept;

  // TxContext &
  // operator=(const TxContext &) noexcept;
  // TxContext &
  // operator=(const TxContext &&) noexcept;

  bool
  handle(dht::MessageContext &) noexcept;

  void
  cancel(dht::DHT &, Tx *) noexcept;
};

void
reset(TxContext &) noexcept;

// dht::TxStore
struct Tx {
  TxContext context;

  Tx *timeout_next;
  Tx *timeout_priv;

  Timestamp sent;

  sp::byte prefix[2];
  sp::byte suffix[2];

  Tx() noexcept;

  Tx(const Tx &) = delete;
  Tx(const Tx &&) = delete;

  Tx &
  operator=(const Tx &) = delete;
  Tx &
  operator=(const Tx &&) = delete;

  bool
  operator==(const krpc::Transaction &) const noexcept;

  explicit operator bool() const noexcept;
};

bool
operator>(const Tx &, const krpc::Transaction &) noexcept;

bool
operator>(const krpc::Transaction &, const Tx &) noexcept;

bool
operator>(const Tx &, const Tx &) noexcept;

} // namespace tx

//=====================================
namespace dht {
// dht::Client
struct Client {
  static constexpr std::size_t tree_capacity = 128;
  fd &udp;
  tx::Tx *timeout_head;

  tx::Tx buffer[tree_capacity];
  binary::StaticTree<tx::Tx> tree;

  std::size_t active;

  void (*deinit)(Client &);

  explicit Client(fd &) noexcept;
  ~Client() noexcept;

  Client(const Client &) = delete;
  Client(const Client &&) = delete;

  Client &
  operator=(const Client &) = delete;
  Client &
  operator=(const Client &&) = delete;
};

} // namespace dht

//=====================================
namespace dht {
// dht::Config
struct Config {
  /* Minimum Node refresh await timeout
   */
  sp::Minutes min_timeout_interval;
  /* Node refresh interval
   */
  sp::Milliseconds refresh_interval;
  /* the max age of a peer db entry before it gets evicted */
  sp::Minutes peer_age_refresh;
  sp::Minutes token_max_age;
  /* Max age of transaction created for outgoing request. Used when reclaiming
   * transaction id. if age is greater than max age then we can reuse the
   * transaction.
   */
  sp::Minutes transaction_timeout;
  /* the generation of find_node request sent to bootstrap our routing table.
   * When max generation is reached we start from zero again. when generation is
   * zero we send find_node(self) otherwise we randomize node id
   */
  std::uint8_t bootstrap_generation_max;
  /* A low water mark indecating when we need to find_nodes to supplement nodes
   * present in our RoutingTable. Is used when comparing active with total
   * nodes if there are < percentage active nodes than /percentage_seek/ we
   * start a search for more nodes.
   */
  std::size_t percentage_seek;
  /* The interval of when a bucket can be used again to send find_node request
   * during the 'look_for_nodes' stage.
   */
  sp::Minutes bucket_find_node_spam;
  /* the number of times during 'look_for_nodes' stage a random bucket is
   * selected and was not used to perform find_node because either it did not
   * contain any good nodes or the bucket where too recently used by
   * 'look_for_nodes'
   */
  std::size_t max_bucket_not_find_node;

  sp::Minutes db_samples_refresh_interval;

  /*  */
  sp::Minutes token_key_refresh;
  /*  */
  sp::Minutes bootstrap_reset;

  Config() noexcept;
};

//=====================================
// dht::Peer
struct Peer {
  Contact contact;
  Timestamp activity;
  bool seed;
  // // {
  // Peer *next;
  // // }
  // {
  Peer *timeout_priv;
  Peer *timeout_next;
  // }
  Peer(Ipv4, Port, Timestamp, bool) noexcept;
  Peer(const Contact &, Timestamp, bool) noexcept;
  // Peer() noexcept;

  // Peer(const Peer &) = delete;
  // Peer(const Peer &&) = delete;

  // Peer &
  // operator=(const Peer &) = delete;
  // Peer &
  // operator=(const Peer &&) = delete;

  bool
  operator==(const Contact &) const noexcept;

  bool
  operator>(const Contact &) const noexcept;

  bool
  operator>(const Peer &) const noexcept;
};

bool
operator>(const Contact &, const Peer &) noexcept;

Timestamp
activity(const Node &) noexcept;

Timestamp
activity(const Peer &) noexcept;

//=====================================
/*dht::Bucket*/
struct Bucket {
  static constexpr std::size_t K = 32;
  Node contacts[K];

  Bucket() noexcept;
  ~Bucket() noexcept;

  Bucket(const Bucket &) = delete;
  Bucket(const Bucket &&) = delete;

  Bucket &
  operator=(const Bucket &) = delete;
  Bucket &
  operator=(const Bucket &&) = delete;
};

template <typename F>
bool
for_all(Bucket &b, F f) noexcept {
  bool result = true;
  for (std::size_t i = 0; i < Bucket::K && result; ++i) {
    if (is_valid(b.contacts[i])) {
      result = f(b.contacts[i]);
    }
  }
  return result;
}

template <typename F>
bool
for_all(const Bucket &b, F f) noexcept {
  bool result = true;
  for (std::size_t i = 0; i < Bucket::K && result; ++i) {
    const Node &current = b.contacts[i];
    if (is_valid(current)) {
      result = f(current);
    }
  }
  return result;
}

template <typename F>
void
for_each(Bucket &b, F f) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    if (is_valid(b.contacts[i])) {
      f(b.contacts[i]);
    }
  }
}

template <typename F>
void
for_each(const Bucket &b, F f) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    const auto &current = b.contacts[i];
    if (is_valid(current)) {
      f(current);
    }
  }
}

//=====================================
// dht::RoutingTable
struct RoutingTable {
  ssize_t depth;
  RoutingTable *in_tree;
  Bucket bucket;
  RoutingTable *next;

  explicit RoutingTable(ssize_t) noexcept;

  RoutingTable(const RoutingTable &) = delete;
  RoutingTable(const RoutingTable &&) = delete;

  RoutingTable &
  operator=(const RoutingTable &) = delete;
  RoutingTable &
  operator=(const RoutingTable &&) = delete;

  ~RoutingTable() noexcept;
};

bool
is_empty(const RoutingTable &) noexcept;

#if 0
bool
operator<(const RoutingTable &, std::size_t) noexcept;

bool
operator<(const RoutingTable &, const RoutingTable &) noexcept;
#endif

struct RoutingTableLess {
  bool
  operator()(const RoutingTable *f, std::size_t s) const noexcept;

  bool
  operator()(const RoutingTable *f, const RoutingTable *s) const noexcept;
};

template <typename F>
bool
for_all(const RoutingTable *it, F f) noexcept {
  while (it) {

    auto it_width = it;
    while (it_width) {

      const RoutingTable &current = *it_width;
      if (!f(current)) {
        return false;
      }
      it_width = it_width->next;
    } // while

    it = it->in_tree;
  } // while

  return true;
}

template <typename F>
bool
for_all_node(const RoutingTable *it, F f) noexcept {
  return for_all(it, [&f](const RoutingTable &r) {
    /**/
    return for_all(r.bucket, f);
  });
}

//=====================================
// dht::KeyValue
struct KeyValue {
  Infohash id;
  sp::SkipList<Peer, 6> peers;
  Peer *timeout_peer;
  char *name;

  explicit KeyValue(const Infohash &) noexcept;

  KeyValue(const KeyValue &) = delete;
  KeyValue(const KeyValue &&) = delete;

  KeyValue &
  operator=(const KeyValue &) = delete;
  KeyValue &
  operator=(const KeyValue &&) = delete;

  ~KeyValue();
};

bool
operator>(const KeyValue &, const Infohash &) noexcept;

bool
operator>(const KeyValue &, const KeyValue &) noexcept;

bool
operator>(const Infohash &, const KeyValue &) noexcept;

//=====================================
// dht::log
struct Log {
  sp::byte id[4];
  Log() noexcept;
};

//=====================================
struct StatTrafic {
  std::size_t ping;
  std::size_t find_node;
  std::size_t get_peers;
  std::size_t announce_peer;
  std::size_t sample_infohashes;
  std::size_t error;

  StatTrafic() noexcept;

  virtual ~StatTrafic() {
  }
};

struct StatDirection {
  StatTrafic request;
  // TODO should only be used in /sent/
  StatTrafic response_timeout;
  StatTrafic response;
  // TODO use StatTrafic for this
  std::size_t parse_error;

  StatDirection() noexcept;
  virtual ~StatDirection() {
  }
};

struct Stat {
  StatDirection sent;
  StatDirection received;

  std::size_t known_tx;
  std::size_t unknown_tx;

  Stat() noexcept;
  virtual ~Stat() {
  }
};

//=====================================
struct SearchContext {
  // ref_cnt is ok since we are in a single threaded ctx
  // incremented for each successfull client::get_peers
  // decremented on receieve & on timeout
  std::size_t ref_cnt;
  // whether this instance should be reclaimd. if ref_cnt is 0
  const Infohash search;
  bool is_dead;

  explicit SearchContext(const Infohash &) noexcept;
};

struct KContact {
  /* number of common prefix bits */
  std::size_t common;
  Contact contact;

  KContact() noexcept
      : common(~std::size_t(0))
      , contact() {
  }

  KContact(std::size_t c, Contact con) noexcept
      : common(c)
      , contact(con) {
  }

  KContact(const Key &in, const Contact &c, const Key &search) noexcept
      : common(rank(in, search))
      , contact(c) {
  }

  KContact(const dht::IdContact &in, const Key &search) noexcept
      : common(rank(in.id, search))
      , contact(in.contact) {
  }

  KContact(const dht::IdContact &in, const NodeId &search) noexcept
      : KContact(in, search.id) {
  }

  explicit operator bool() const noexcept {
    return common != ~std::size_t(0);
  }

  bool
  operator>(const KContact &o) const noexcept {
    assertx(o.common != ~std::size_t(0));
    assertx(common != ~std::size_t(0));
    return common > o.common;
  }
};

struct Search {
  SearchContext *ctx;
  Infohash search;

  sp::StaticArray<sp::hasher<NodeId>, 2> hashers;
  sp::BloomFilter<NodeId, 8 * 1024 * 1024> searched;
  sp::Timestamp timeout;

  heap::StaticMaxBinary<KContact, 1024> queue;
  sp::LinkedList<Contact> result;

  Search *next;
  Search *priv;

  std::size_t fail;

  explicit Search(const Infohash &) noexcept;

#if 0
  Search(Search &&) noexcept;
#endif

  Search(const Search &&) = delete;
  Search(const Search &) = delete;

  Search &
  operator=(const Search &) = delete;
  Search &
  operator=(const Search &&) = delete;

  ~Search() noexcept;
};

bool
operator>(const Infohash &, const Search &) noexcept;

bool
operator>(const Search &, const Search &) noexcept;

bool
operator>(const Search &, const Infohash &) noexcept;

//=====================================
struct TokenKey {
  uint32_t key;
  Timestamp created;
  TokenKey() noexcept;
};

// dht::DHT
struct DHT {
  // self {{{
  NodeId id;
  Client client;
  sp::fd priv_fd;
  Log log;
  Contact ip;
  prng::xorshift32 &random;
  sp::ip_election election;
  Stat statistics;
  std::size_t ip_cnt;
  Config config;
  sp::core core;
  bool should_exit;
  //}}}

  // peer-lookup db {{{
  struct {
    avl::Tree<KeyValue> lookup_table;
    TokenKey key[2];
    uint32_t activity = {0};
    uint32_t length_lookup_table = {0};
    Timestamp last_generated{0};
    sp::UinStaticArray<dht::Infohash, 20> random_samples;
  } db;
  //}}}

  // routing-table {{{
  RoutingTable *root;
  std::size_t root_limit;
  // TODO dynamic
  heap::StaticBinary<RoutingTable *, 1024, RoutingTableLess> rt_reuse;
  sp::dstack<RoutingTable *> root_extra;
  //}}}

  // timeout {{{
  Timestamp timeout_next;
  Node *timeout_node;
  //}}}

  // recycle contact list {{{
  // sp::UinStaticArray<Node, 256> recycle_node_list;
  sp::UinStaticArray<Contact, 256> recycle_contact_list;
  // sp::UinStaticArray<IdContact, 256> recycle_id_contact_list;
  // }}}

  // stuff {{{
  Timestamp last_activity;

  std::uint32_t total_nodes;
  std::uint32_t bad_nodes;
  Timestamp now;
  // }}}

  // boostrap {{{
  Timestamp bootstrap_last_reset;
  sp::StaticArray<sp::hasher<Ip>, 2> bootstrap_hashers;
  sp::BloomFilter<Ip, 8 * 1024> bootstrap_filter;
  heap::StaticMaxBinary<KContact, 128> bootstrap;
  std::uint32_t active_find_nodes;
  // }}}

  // priv interface searches {{{
  Search *search_root;
  avl::Tree<Search> searches;
  // }}}

  // upnp {{{
  Timestamp upnp_sent;
  // }}}

  //  {{{
  void (*retire_good)(DHT &, Contact) noexcept = nullptr;
  void (*topup_bootstrap)(DHT &) noexcept = nullptr;
  void *cache{nullptr};
  // }}}

  DHT(fd &, fd &priv_fd, const Contact &self, prng::xorshift32 &, Timestamp now)
  noexcept;

  DHT(const DHT &) = delete;
  DHT(const DHT &&) = delete;

  DHT &
  operator=(const DHT &) = delete;
  DHT &
  operator=(const DHT &&) = delete;

  ~DHT();
};

//=====================================
// dht::MessageContext
struct MessageContext {
  dht::Domain domain;
  const char *query;

  DHT &dht;

  sp::Buffer &in;
  sp::Buffer &out;

  const krpc::Transaction &transaction;
  Contact remote;
  sp::maybe<Contact> ip_vote;
  bool read_only;

  MessageContext(DHT &, const krpc::ParseContext &, sp::Buffer &,
                 Contact) noexcept;

  MessageContext(const MessageContext &) = delete;
  MessageContext(const MessageContext &&) = delete;

  MessageContext &
  operator=(const MessageContext &) = delete;
  MessageContext &
  operator=(const MessageContext &&) = delete;
};

//=====================================
} // namespace dht

#endif
