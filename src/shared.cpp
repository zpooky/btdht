#include "shared.h"
#include "search.h"
#include <algorithm>
#include <cstring>
#include <util/assert.h>
#include <utility>

#include <hash/djb2.h>
#include <hash/fnv.h>

//=====================================
namespace krpc {
// krpc::ParseContext
ParseContext::ParseContext(dht::Domain domain, dht::DHT &ictx,
                           sp::Buffer &d) noexcept
    : domain{domain}
    , ctx{ictx}
    , decoder(d)
    , tx()
    , msg_type{0}
    , query{0}
    , remote_version{0}
    , read_only{0}
    , ip_vote{} {
}

ParseContext::ParseContext(ParseContext &ictx, sp::Buffer &d) noexcept
    : domain{ictx.domain}
    , ctx{ictx.ctx}
    , decoder(d)
    , tx(ictx.tx)
    , msg_type{0}
    , query{0}
    , remote_version{0}
    , read_only{ictx.read_only}
    , ip_vote{} {

  std::memcpy(msg_type, ictx.msg_type, sizeof(msg_type));
  std::memcpy(query, ictx.query, sizeof(query));
  std::memcpy(remote_version, ictx.remote_version, sizeof(remote_version));
}
} // namespace krpc

//=====================================
namespace tx {
void
TxContext::cancel(dht::DHT &dht, Tx *tx) noexcept {
  assertx(tx);
  if (int_cancel) {
    krpc::Transaction t(tx->prefix, tx->suffix);
    int_cancel(dht, t, tx->sent, closure);
  }
}

bool
TxContext::handle(dht::MessageContext &ctx) noexcept {
  assertx(int_handle);
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

//=====================================
namespace dht {
/*dht::Client*/
Client::Client(fd &fd) noexcept
    : udp(fd)
    , timeout_head(nullptr)
    , buffer{}
    , tree{buffer}
    , active(0)
    , deinit(nullptr) {
}

Client::~Client() noexcept {
  if (deinit) {
    deinit(*this);
  }
}

} // namespace dht

//=====================================
namespace dht {
/*dht::Config*/
Config::Config() noexcept
    // seconds
    : min_timeout_interval(1)
    , refresh_interval(sp::Minutes(15))
    , peer_age_refresh(45)
    , token_max_age(15)
    , transaction_timeout(2)
    //
    , bootstrap_generation_max(16)
    , percentage_seek(40)
    //
    , bucket_find_node_spam(1)
    , max_bucket_not_find_node(5)
    //
    , db_samples_refresh_interval(60)
    //
    , token_key_refresh(15)
    //
    , bootstrap_reset(60)
//
{
}

/*dht::Peer*/
Peer::Peer(Ipv4 i, Port p, Timestamp n, bool s) noexcept
    : contact(i, p)
    , activity(n)
    , seed(s)
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

Peer::Peer(const Contact &c, Timestamp a, bool s) noexcept
    : contact(c)
    , activity(a)
    , seed(s)
    //{
    , timeout_priv(nullptr)
    , timeout_next(nullptr)
//}
{
}

// Peer::Peer() noexcept
//     : Peer(0, 0, Timestamp(0), false) {
// }

bool
Peer::operator==(const Contact &c) const noexcept {
  return contact.operator==(c);
}

bool
Peer::operator>(const Contact &p) const noexcept {
  return contact > p;
}

bool
Peer::operator>(const Peer &p) const noexcept {
  return operator>(p.contact);
}

bool
operator>(const Contact &f, const Peer &s) noexcept {
  return f > s.contact;
}

Timestamp
activity(const Node &head) noexcept {
  return head.remote_activity;
}

Timestamp
activity(const Peer &peer) noexcept {
  return peer.activity;
}

//=====================================
/*dht::Bucket*/
Bucket::Bucket() noexcept
    : contacts() {
}

Bucket::~Bucket() noexcept {
}

//=====================================
/*dht::RoutingTable*/
RoutingTable::RoutingTable(ssize_t d) noexcept
    : depth(d)
    , in_tree()
    , bucket()
    , next(nullptr) {
}

RoutingTable::~RoutingTable() noexcept {
  if (in_tree) {
    delete in_tree;
    in_tree = nullptr;
  }
}

bool
is_empty(const RoutingTable &root) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    const auto &current = root.bucket.contacts[i];
    if (is_valid(current)) {
      return false;
    }
  }
  return true;
}

#if 0
bool
operator<(const RoutingTable &f, std::size_t depth) noexcept {
  return f.depth < depth;
}

bool
operator<(const RoutingTable &f, const RoutingTable &s) noexcept {
  return f.depth < s.depth;
}
#endif

bool
RoutingTableLess::operator()(const RoutingTable *f,
                             std::size_t depth) const noexcept {
  assertx(f);
  if (f->depth < 0) {
    return true;
  }

  return f->depth < depth;
}

bool
RoutingTableLess::operator()(const RoutingTable *f,
                             const RoutingTable *s) const noexcept {
  assertx(f);
  assertx(s);
  return f->depth < s->depth;
}

//=====================================
// dht::KeyValue
KeyValue::KeyValue(const Infohash &pid) noexcept
    : id(pid)
    , peers{}
    , timeout_peer{nullptr}
    , name{'\0'} {
}

KeyValue::~KeyValue() {
}

bool
operator>(const KeyValue &self, const Infohash &o) noexcept {
  return self.id > o.id;
}

bool
operator>(const KeyValue &self, const KeyValue &o) noexcept {
  return self.id > o.id.id;
}

bool
operator>(const Infohash &f, const KeyValue &s) noexcept {
  return f > s.id.id;
}

//=====================================
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

SearchContext::SearchContext(const Infohash &s) noexcept
    : ref_cnt(1)
    , search(s)
    , is_dead(false) {
}

//=====================================
Search::Search(const Infohash &s) noexcept
    : ctx(new SearchContext(s))
    , search(s)
    , hashers()
    , searched(hashers)
    , timeout(0)
    , queue()
    , result()
    , next(nullptr)
    , priv(nullptr)
    , fail(0) {

  auto djb_f = [](const NodeId &id) -> std::size_t {
    return djb2::encode32(id.id, sizeof(id.id));
  };

  auto fnv_f = [](const NodeId &id) -> std::size_t {
    return fnv_1a::encode64(id.id, sizeof(id.id));
  };

  assertx_n(insert(hashers, djb_f));
  assertx_n(insert(hashers, fnv_f));
}

#if 0
Search::Search(Search &&o) noexcept
    : ctx(nullptr)
    , search()
    , remote()
    , hashers()
    , searched()
    , timeout()
    , queue()
    , result()
    , search_next(nullptr) {

  using std::swap;
  swap(ctx, o.ctx);
  swap(search, o.search);
  swap(remote, o.remote);
  swap(hashers, o.hashers);
  swap(searched, o.searched);
  swap(timeout, o.timeout);
  swap(queue, o.queue);
  swap(result, o.result);
  swap(search_next, o.search_next);
}
#endif

Search::~Search() noexcept {
  if (ctx) {
    ctx->is_dead = true;
    search_decrement(ctx);
  }
}

bool
operator>(const Infohash &f, const Search &o) noexcept {
  return f > o.search;
}

bool
operator>(const Search &f, const Search &s) noexcept {
  return f.search > s.search;
}

bool
operator>(const Search &f, const Infohash &s) noexcept {
  return f.search > s;
}

//=====================================
#if 0
Search::Search(Search &&o) noexcept
    : ctx(nullptr)
    , search(o.search)
    , remote(o.remote)
    , hashers()
    , searched(hashers)
    , timeout(0)
    , raw_queue(nullptr)
    , queue(nullptr, 0) {
  using sp::swap;

  swap(ctx, o.ctx);
  insert_all(hashers, o.hashers);
  swap(searched, o.searched);
  swap(timeout, o.timeout);
}
#endif

TokenKey::TokenKey() noexcept
    : key{}
    , created{0} {
}

// dht::DHT
DHT::DHT(fd &udp, fd &p_priv_fd, const Contact &self, prng::xorshift32 &r,
         Timestamp n) noexcept
    // self {{{
    : id()
    , client(udp)
    , log()
    , ip(self)
    , random(r)
    , election()
    , statistics()
    , ip_cnt(0)
    , config()
    , core()
    , should_exit(false)
    //}}}
    // peer-lookup db {{{
    // , db::lookup_table()
    // , db::key{} //}}}
    // routing-table {{{
    , root(nullptr)
    , root_limit(4)
    , rt_reuse()
    , root_extra()
    //}}}
    // timeout{{{
    , timeout_next(0)
    , timeout_node(nullptr)
    //}}}
    // recycle contact list {{{
    , recycle_contact_list()
    , recycle_id_contact_list()
    // }}}
    // {{{
    /*timestamp of received request&response*/
    , last_activity(0)
    /*total nodes present in the routing table*/
    , total_nodes(0)
    , bad_nodes(0)
    , now(n)
    // boostrap {{{
    , bootstrap_last_reset(0)
    , bootstrap_hashers()
    , bootstrap_filter(bootstrap_hashers)
    , bootstrap()
    , active_find_nodes(0)
    // }}}
    // searches{{{
    , search_root(nullptr)
    , searches()
    //}}}
    // upnp{{{
    , upnp_sent{0} {

  assertx_n(insert(bootstrap_hashers, djb_ip));
  assertx_n(insert(bootstrap_hashers, fnv_ip));

  for (size_t i = 0; i < 2; ++i) {
    fill(r, &this->db.key[i].key, sizeof(this->db.key[i].key));
    this->db.key[i].created = now;
  }
}

DHT::~DHT() {
  // TODO reclaim
}

//=====================================
/*dht::MessageContext*/
MessageContext::MessageContext(DHT &p_dht, const krpc::ParseContext &ctx,
                               sp::Buffer &p_out, Contact p_remote) noexcept
    : domain(ctx.domain)
    , query(ctx.query)
    , dht{p_dht}
    , in{ctx.decoder}
    , out{p_out}
    , transaction{ctx.tx}
    , remote{p_remote}
    , ip_vote(ctx.ip_vote)
    , read_only{ctx.read_only} {
  assertx(bool(ip_vote) == bool(ctx.ip_vote));
}

} // namespace dht
  //=====================================
