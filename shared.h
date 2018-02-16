#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

#include "ip_election.h"
#include "util.h"
#include <cstdio> //debug
#include <cstdlib>

#include "bencode.h"
#include <list/FixedList.h>
#include <tree/StaticTree.h>

#include <prng/xorshift.h>
#include <util/maybe.h>

//---------------------------
namespace krpc {
// krpc::ParseContext
struct ParseContext {
  bencode::d::Decoder &decoder;
  Transaction tx;

  char msg_type[16];
  char query[16];
  sp::byte remote_version[16];

  sp::maybe<Contact> ip_vote;

  explicit ParseContext(bencode::d::Decoder &) noexcept;

  ParseContext(ParseContext &, bencode::d::Decoder &) noexcept;
};

} // namespace krpc

//---------------------------
namespace dht {

struct MessageContext;
struct DHT;
} // namespace dht

namespace tx {

using TxCancelHandle = void (*)(dht::DHT &, void *) noexcept;
using TxHandle = bool (*)(dht::MessageContext &, void *) noexcept;
// dht::TxContext
struct TxContext {
  TxHandle int_handle;
  TxCancelHandle int_cancel;
  void *closure;

  TxContext(TxHandle, TxCancelHandle, void *) noexcept;
  TxContext() noexcept;

  bool
  handle(dht::MessageContext &) noexcept;

  void
  cancel(dht::DHT &) noexcept;
};

void
reset(TxContext &) noexcept;

// dht::TxStore
struct Tx {
  TxContext context;

  Tx *timeout_next;
  Tx *timeout_priv;

  time_t sent;

  sp::byte prefix[2];
  sp::byte suffix[2];

  Tx() noexcept;

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

namespace dht {
// dht::Client
struct Client {
  static constexpr std::size_t tree_capacity = 128;
  fd &udp;
  tx::Tx *timeout_head;

  tx::Tx buffer[tree_capacity];
  binary::StaticTree<tx::Tx> tree;

  std::size_t active;

  explicit Client(fd &) noexcept;
};

} // namespace dht

//---------------------------
namespace dht {
// dht::Config
struct Config {
  // TODO change from time_t since time_t is a abs timestamp?
  /*
   * Min Node refresh await timeout
   */
  time_t min_timeout_interval;
  /*
   * Node refresh interval
   */
  Timeout refresh_interval;
  time_t peer_age_refresh;
  time_t token_max_age;
  /*
   * Max age of transaction created for outgoing request. Used when reclaiming
   * transaction id. if age is greater than max age then we can reuse the
   * transaction.
   */
  time_t transaction_timeout;
  /*
   * the generation of find_node request sent to bootstrap our routing table.
   * When max generation is reached we start from zero again. when generation is
   * zero we send find_node(self) otherwise we randomize node id
   */
  std::uint8_t bootstrap_generation_max;
  /*
   * A low water mark indecating when we need to find_nodes to supplement nodes
   * present in our RoutingTable. Is used when comparing active with total
   * nodes if there are < percentage active nodes than /percentage_seek/ we
   * start a search for more nodes.
   */
  std::size_t percentage_seek;
  /*
   * The interval of when a bucket can be used again to send find_node request
   * during the 'look_for_nodes' stage.
   */
  std::size_t bucket_find_node_spam;
  /*
   * the number of times during 'look_for_nodes' stage a random bucket is
   * selected and was not used to perform find_node because either it did not
   * contain any good nodes or the bucket where too recently used by
   * 'look_for_nodes'
   */
  std::size_t max_bucket_not_find_node;

  Config() noexcept;
};

// dht::Infohash
struct Infohash {
  Key id;

  Infohash() noexcept;

  bool
  operator==(const Infohash &) const noexcept;
};

// dht::Peer
struct Peer {
  Contact contact;
  time_t activity;
  // {
  Peer *next;
  // }
  // {
  Peer *timeout_priv;
  Peer *timeout_next;
  // }
  Peer(Ipv4, Port, time_t) noexcept;
  Peer(const Contact &, time_t, Peer *next) noexcept;
  Peer() noexcept;

  bool
  operator==(const Contact &) const noexcept;
};

template <typename F>
static bool
for_all(const dht::Peer *l, F f) noexcept {
  while (l) {
    if (!f(*l)) {
      return false;
    }
    l = l->next;
  }
  return true;
}

time_t
activity(const Node &) noexcept;

time_t
activity(const Peer &) noexcept;

/*dht::Bucket*/
struct Bucket {
  static constexpr std::size_t K = 32;
  Node contacts[K];
  std::time_t find_node;
  std::uint8_t bootstrap_generation;

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
    if (b.contacts[i]) {
      result = f(b.contacts[i]);
    }
  }
  return result;
}

template <typename F>
void
for_each(Bucket &b, F f) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    if (b.contacts[i]) {
      f(b.contacts[i]);
    }
  }
}

// dht::RoutingTable
struct RoutingTable {
  RoutingTable *in_tree;
  Bucket bucket;

  RoutingTable() noexcept;

  ~RoutingTable() noexcept;
};

template <typename F>
bool
for_all(const RoutingTable *it, F f) noexcept {
  bool result = true;
  while (result && it) {
    result = f(*it);
    it = it->in_tree;
  }
  return result;
}

// dht::KeyValue
struct KeyValue {
  KeyValue *next;
  Peer *peers;
  Infohash id;
  //
  KeyValue(const Infohash &, KeyValue *) noexcept;
};

template <typename F>
bool
for_all(const dht::KeyValue *it, F f) noexcept {
  bool ret = true;
  while (it && ret) {
    ret = f(*it);
  }
  return ret;
} // bencode::e::for_all()

// dht::log
struct Log {
  sp::byte id[4];
  Log() noexcept;
};

struct StatTrafic {
  std::size_t ping;
  std::size_t find_node;
  std::size_t get_peers;
  std::size_t announce_peer;
  std::size_t error;

  StatTrafic() noexcept;
};

struct StatDirection {
  StatTrafic request;
  // TODO should only be used in /sent/
  StatTrafic response_timeout;
  StatTrafic response;
  // TODO use StatTrafic for this
  std::size_t parse_error;

  StatDirection() noexcept;
};

struct Stat {
  StatDirection sent;
  StatDirection received;

  std::size_t known_tx;
  std::size_t unknown_tx;

  Stat() noexcept;
};

// dht::DHT
struct DHT {
  static const std::size_t token_table = 64;
  // self {{{
  NodeId id;
  Client client;
  Log log;
  Contact ip;
  prng::Xorshift32 &random;
  sp::ip_election election;
  Stat statistics;
  std::size_t ip_cnt;
  //}}}
  // peer-lookup db {{{
  KeyValue *lookup_table;
  Peer *timeout_peer;
  time_t timeout_peer_next;
  //}}}
  // routing-table {{{
  RoutingTable *root;
  //}}}
  // timeout {{{
  time_t timeout_next;
  Node *timeout_node;
  //}}}
  // recycle contact list {{{
  sp::list<Node> recycle_contact_list;
  sp::list<Contact> recycle_value_list;
  // }}}
  // stuff {{{
  time_t last_activity;

  std::uint32_t total_nodes;
  std::uint32_t bad_nodes;
  time_t now;
  // }}}
  // boostrap {{{
  sp::list<Contact> bootstrap_contacts;
  std::uint32_t active_searches;
  // }}}

  explicit DHT(fd &, const Contact &, prng::Xorshift32 &) noexcept;

  DHT(const DHT &) = delete;
  DHT(const DHT &&) = delete;

  DHT &
  operator=(const DHT &) = delete;
  DHT &
  operator=(const DHT &&) = delete;
};

// dht::MessageContext
struct MessageContext {
  const char *query;

  DHT &dht;

  bencode::d::Decoder &in;
  sp::Buffer &out;

  const krpc::Transaction &transaction;
  Contact remote;
  sp::maybe<Contact> ip_vote;

  MessageContext(DHT &, const krpc::ParseContext &, sp::Buffer &,
                 Contact) noexcept;
};
} // namespace dht

#endif
