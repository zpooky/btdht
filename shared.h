#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

#include "util.h"
#include <stdio.h> //debug
#include <stdlib.h>

#include "bencode.h"

//---------------------------
namespace krpc {
// krpc::ParseContext
struct ParseContext {
  bencode::d::Decoder &decoder;
  Transaction tx;

  char msg_type[16];
  char query[16];
  sp::byte remote_version[16];
  sp::byte ext_ip[16];

  explicit ParseContext(bencode::d::Decoder &) noexcept;

  ParseContext(ParseContext &, bencode::d::Decoder &) noexcept;
};

} // namespace krpc

//---------------------------
namespace dht {

struct MessageContext;
struct DHT;

using TxCancelHandle = void (*)(DHT &, void *) noexcept;
using TxHandle = bool (*)(MessageContext &, void *) noexcept;

// dht::TxContext
struct TxContext {
  TxHandle int_handle;
  TxCancelHandle int_cancel;
  void *closure;

  TxContext(TxHandle, TxCancelHandle, void *) noexcept;
  TxContext() noexcept;

  bool
  handle(MessageContext &) noexcept;

  void
  cancel(DHT &) noexcept;
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

  int
  cmp(const krpc::Transaction &) const noexcept;
};

template <std::size_t level>
struct static_breadth {
  static constexpr std::size_t value = std::size_t(std::size_t(1) << level);
};

template <>
struct static_breadth<0> {
  static constexpr std::size_t value = 1;
};

template <std::size_t level>
struct static_size {
  static constexpr std::size_t value =
      static_breadth<level>::value + static_size<level - 1>::value;
};
template <>
struct static_size<0> {
  static constexpr std::size_t value = 1;
};

// dht::TxTree
struct TxTree {
  static constexpr std::size_t levels = 7;
  static constexpr std::size_t capacity = static_size<levels>::value;
  Tx storagex[capacity];

  TxTree() noexcept;

  Tx &operator[](std::size_t) noexcept;
};

// dht::Client
struct Client {
  fd &udp;
  TxTree tree;
  Tx *timeout_head;

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
  std::uint8_t percentage_seek;

  Config() noexcept;
};

// dht::Infohash
struct Infohash {
  Key id;

  Infohash();

  bool
  operator==(const Infohash &) const noexcept;
};

// dht::Token
struct Token {
  sp::byte id[20];
  Token();
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
  static constexpr std::size_t K = 8;
  Node contacts[K];
  std::uint8_t bootstrap_generation;

  Bucket();
  ~Bucket();
};

template <typename F>
bool
for_all(Bucket &b, F f) {
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
for_each(Bucket &b, F f) {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    if (b.contacts[i]) {
      f(b.contacts[i]);
    }
  }
}

// dht::RoutingTable
enum class NodeType { NODE, LEAF };
struct RoutingTable {
  union {
    struct {
      RoutingTable *higher;
      RoutingTable *lower;
      Key middle;
    } node;
    Bucket bucket;
  };
  NodeType type;

  RoutingTable(RoutingTable *h, RoutingTable *l);
  RoutingTable();

  ~RoutingTable();
};

// dht::KeyValue
struct KeyValue {
  KeyValue *next;
  Peer *peers;
  Infohash id;
  //
  KeyValue(const Infohash &, KeyValue *);
};

// dht::log
struct Log {
  sp::byte id[4];
  Log();
};

// dht::DHT
struct DHT {
  static const std::size_t token_table = 64;
  // self {{{
  NodeId id;
  Client client;
  Log log;
  Contact ip;
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
  sp::list<Node> contact_list;
  sp::list<Contact> value_list;
  // }}}
  // stuff {{{
  time_t last_activity;

  std::uint32_t total_nodes;
  std::uint32_t bad_nodes;
  time_t now;
  // }}}
  // boostrap {{{
  sp::list<Contact> bootstrap_contacts;
  std::size_t bootstrap_ongoing_searches;
  // }}}

  explicit DHT(fd &, const Contact &);
};

// dht::MessageContext
struct MessageContext {
  const char *query;

  DHT &dht;

  bencode::d::Decoder &in;
  sp::Buffer &out;

  const krpc::Transaction &transaction;
  Contact remote;
  MessageContext(DHT &, const krpc::ParseContext &, sp::Buffer &,
                 Contact) noexcept;
};
} // namespace dht

#endif
