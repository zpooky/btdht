#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

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

#include "db.h"
#include "ip_election.h"
#include "routing_table.h"
#include "search.h"
#include "timeout.h"
#include "util.h"

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
                                const Timestamp &, void *);
using TxHandle = bool (*)(dht::MessageContext &, void *);
// dht::TxContext
struct TxContext {
  TxHandle int_handle;
  TxCancelHandle int_timeout;
  void *closure;

  sp::Timestamp latency;

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
  timeout(dht::DHT &, Tx *) noexcept;
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

struct DHTMetaScrape {
  dht::DHTMetaRoutingTable routing_table;
  DHTMetaScrape(dht::DHT &, const dht::NodeId &) noexcept;
  virtual ~DHTMetaScrape() {
  }
};

//=====================================
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

  db::DHTMetaDatabase db;
  dht::DHTMetaRoutingTable routing_table;

  // timeout {{{
  timeout::Timeout timeout;
  //}}}

  // recycle contact list {{{
  // sp::UinStaticArray<Node, 256> recycle_node_list;
  sp::UinStaticArray<Contact, 256> recycle_contact_list;
  // sp::UinStaticArray<IdContact, 256> recycle_id_contact_list;
  // }}}

  // stuff {{{
  Timestamp last_activity;

  Timestamp &now;
  // }}}

  // boostrap {{{
  Timestamp bootstrap_last_reset;
  sp::StaticArray<sp::hasher<Ip>, 2> ip_hashers;
  sp::BloomFilter<Ip, 8 * 1024> bootstrap_filter;
  heap::StaticMaxBinary<KContact, 128> bootstrap;
  // }}}

  DHTMetaSearch searches;
  // struct {
  sp::UinStaticArray<DHTMetaScrape, 8> scrapes;
  sp::UinStaticArray<sp::BloomFilter<Ip, 8 * 1024>, 24> scrape_hour;
  std::size_t scrape_hour_i;
  Timestamp scrape_hour_time;
  // } scrape;

  // upnp {{{
  Timestamp upnp_sent;
  // }}}

  //  {{{
  void (*topup_bootstrap)(DHT &) noexcept = nullptr;
  // }}}

  DHT(fd &, fd &priv_fd, const Contact &self, prng::xorshift32 &,
      Timestamp &now)
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
  // sp::maybe<Contact> ip_vote;
  bool read_only;

  krpc::ParseContext &pctx;

  MessageContext(DHT &, krpc::ParseContext &, sp::Buffer &, Contact) noexcept;

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
