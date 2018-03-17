#include "shared.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

#include <hash/djb2.h>
#include <hash/fnv.h>
//
//---------------------------
namespace krpc {
// krpc::ParseContext
ParseContext::ParseContext(sp::Buffer &d) noexcept
    : decoder(d)
    , tx()
    , msg_type{0}
    , query{0}
    , remote_version{0}
    , ip_vote{} {
}

ParseContext::ParseContext(ParseContext &ctx, sp::Buffer &d) noexcept
    : decoder(d)
    , tx(ctx.tx)
    , msg_type{0}
    , query{0}
    , remote_version{0}
    , ip_vote{} {

  std::memcpy(msg_type, ctx.msg_type, sizeof(msg_type));
  std::memcpy(query, ctx.query, sizeof(query));
  std::memcpy(remote_version, ctx.remote_version, sizeof(remote_version));
}
} // namespace krpc

//---------------------------
namespace tx {

void
TxContext::cancel(dht::DHT &dht) noexcept {
  if (int_cancel) {
    int_cancel(dht, closure);
  }
}

bool
TxContext::handle(dht::MessageContext &ctx) noexcept {
  assert(int_handle);
  return int_handle(ctx, closure);
}

TxContext::TxContext(TxHandle h, TxCancelHandle ch, void *c) noexcept
    : int_handle(h)
    , int_cancel(ch)
    , closure(c) {
}

TxContext::TxContext() noexcept
    : TxContext(nullptr, nullptr, nullptr) {
}

void
reset(TxContext &ctx) noexcept {
  ctx.int_handle = nullptr;
  ctx.int_cancel = nullptr;
  ctx.closure = nullptr;
}

/*dht::Tx*/
Tx::Tx() noexcept
    : context{nullptr, nullptr, nullptr}
    , timeout_next(nullptr)
    , timeout_priv(nullptr)
    , sent(0)
    , prefix{0}
    , suffix{0} {
}

bool
Tx::operator==(const krpc::Transaction &tx) const noexcept {
  constexpr std::size_t p_len = sizeof(prefix);
  constexpr std::size_t s_len = sizeof(suffix);

  if (tx.length == (p_len + s_len)) {
    if (std::memcmp(tx.id, prefix, p_len) == 0) {
      if (std::memcmp(tx.id + p_len, suffix, s_len) == 0) {
        return true;
      }
    }
  }
  return false;
}

Tx::operator bool() const noexcept {
  sp::byte zero[2] = {0};
  return !(std::memcmp(prefix, zero, sizeof(zero)) == 0);
}

bool
operator>(const Tx &self, const krpc::Transaction &o) noexcept {
  return std::memcmp(self.prefix, o.id, sizeof(self.prefix)) > 0;
}

bool
operator>(const krpc::Transaction &self, const Tx &o) noexcept {
  return std::memcmp(self.id, o.prefix, sizeof(o.prefix)) > 0;
}

bool
operator>(const Tx &self, const Tx &tx) noexcept {
  return std::memcmp(self.prefix, tx.prefix, sizeof(self.prefix)) > 0;
}
} // namespace tx

namespace dht {
/*dht::Client*/
Client::Client(fd &fd) noexcept
    : udp(fd)
    , timeout_head(nullptr)
    , buffer{}
    , tree{buffer}
    , active(0) {
}

} // namespace dht
//---------------------------
namespace dht {
/*dht::Config*/
Config::Config() noexcept
    // seconds
    : min_timeout_interval(1)
    , refresh_interval(sp::Minutes(15))
    , peer_age_refresh(45)
    , token_max_age(15)
    , transaction_timeout(1)
    //
    , bootstrap_generation_max(16)
    , percentage_seek(10)
    //
    , bucket_find_node_spam(1)
    , max_bucket_not_find_node(5)
//
{
}

// dht::Infohash
Infohash::Infohash() noexcept
    : id{0} {
}

bool
Infohash::operator==(const Infohash &o) const noexcept {
  return std::memcmp(id, o.id, sizeof(id)) == 0;
}

/*dht::Peer*/
Peer::Peer(Ipv4 i, Port p, Timestamp n) noexcept
    : contact(i, p)
    , activity(n)
    //{
    , next(nullptr)
    //}
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

Peer::Peer(const Contact &c, Timestamp a, Peer *nxt) noexcept
    : contact(c)
    , activity(a)
    //{
    , next(nxt)
    //}
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

Peer::Peer() noexcept
    : Peer(0, 0, Timestamp(0)) {
}

bool
Peer::operator==(const Contact &c) const noexcept {
  return contact.operator==(c);
}

Timestamp
activity(const Node &head) noexcept {
  return std::max(head.request_activity, head.response_activity);
}

Timestamp
activity(const Peer &peer) noexcept {
  return peer.activity;
}

/*dht::Bucket*/
Bucket::Bucket() noexcept
    : contacts()
    , find_node(0)
    , bootstrap_generation(0) {
}

Bucket::~Bucket() noexcept {
}

/*dht::RoutingTable*/
RoutingTable::RoutingTable() noexcept
    : in_tree()
    , bucket() {
}

RoutingTable::~RoutingTable() noexcept {
  if (in_tree) {
    delete in_tree;
    in_tree = nullptr;
  }
}

// dht::KeyValue
KeyValue::KeyValue(const Infohash &pid, KeyValue *nxt) noexcept
    : next(nxt)
    , peers(nullptr)
    , id() {
  std::memcpy(id.id, pid.id, sizeof(id.id));
}

// dht::Log
Log::Log() noexcept
    : id{0} {
}

StatTrafic::StatTrafic() noexcept
    : ping(0)
    , find_node(0)
    , get_peers(0)
    , announce_peer(0)
    , error(0) {
}

StatDirection::StatDirection() noexcept
    : request()
    , response_timeout()
    , response()
    , parse_error(0) {
}

Stat::Stat() noexcept
    : sent()
    , received()
    , known_tx()
    , unknown_tx() {
}

SearchContext::SearchContext() noexcept
    : ref_cnt(0)
    , is_dead(false) {
}

Search::Search(const Infohash &s) noexcept
    : ctx(new SearchContext)
    , search(s)
    , hashers()
    , searched(hashers)
    , started(sp::now())
    , queue() {

  assert(insert(hashers, djb2a::hash<NodeId>));
  assert(insert(hashers, fnv_1a::hash<NodeId>));
}

Search *
find_search(dht::DHT &dht, SearchContext *needle) noexcept {
  sp::LinkedList<Search> &ctx = dht.searches;
  assert(needle);
  return find_first(ctx, [&](const Search &current) {
    /**/
    return current.ctx == needle;
  });
}

// dht::DHT
DHT::DHT(fd &udp, const Contact &i, prng::xorshift32 &r) noexcept
    // self {{{
    : id()
    , client(udp)
    , log()
    , ip(i)
    , random(r)
    , election()
    , statistics()
    , ip_cnt(0)
    //}}}
    // peer-lookup db {{{
    , lookup_table(nullptr)
    , timeout_peer(nullptr)
    , timeout_peer_next(0)
    //}}}
    // routing-table {{{
    , root(nullptr)
    //}}}
    // timeout{{{
    , timeout_next(0)
    , timeout_node(nullptr)
    //}}}
    // recycle contact list {{{
    , recycle_contact_list()
    , recycle_value_list()
    // }}}
    // {{{
    /*timestamp of received request&response*/
    , last_activity(0)
    /*total nodes present in the routing table*/
    , total_nodes(0)
    , bad_nodes(0)
    , now(0)
    // boostrap {{{
    , bootstrap_contacts()
    , active_searches(0)
    // }}}
    // searches{{{
    , searches()
//}}}

//}}}
{
}

/*dht::MessageContext*/
MessageContext::MessageContext(DHT &p_dht, const krpc::ParseContext &ctx,
                               sp::Buffer &p_out, Contact p_remote) noexcept
    : query(ctx.query)
    , dht{p_dht}
    , in{ctx.decoder}
    , out{p_out}
    , transaction{ctx.tx}
    , remote{p_remote}
    , ip_vote(ctx.ip_vote) {
  assert(bool(ip_vote) == bool(ctx.ip_vote));
}

} // namespace dht
