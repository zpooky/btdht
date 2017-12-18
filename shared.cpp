#include "shared.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdio.h>
#include <utility>

//---------------------------
namespace dht {
/*dht::Tx*/
Tx::Tx() noexcept
    : handle(nullptr)
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

int
Tx::cmp(const krpc::Transaction &tx) const noexcept {
  return std::memcmp(prefix, tx.id, sizeof(prefix));
}

/*dht::TxTree*/
TxTree::TxTree() noexcept
    : storagex{} {
}

Tx &TxTree::operator[](std::size_t idx) noexcept {
  assert(idx < capacity);
  return storagex[idx];
}

/*dht::Client*/
Client::Client(fd &fd) noexcept
    : udp(fd)
    , tree()
    , timeout_head(nullptr) {
}

} // namespace dht
//---------------------------
namespace dht {
/*dht::Config*/
Config::Config() noexcept
    // seconds
    : min_timeout_interval(60)
    , refresh_interval(15 * 60)
    , peer_age_refresh(60 * 45)
    , token_max_age(15 * 60) {
}

bool
is_valid(const NodeId &id) noexcept {
  constexpr Key allzeros = {0};
  return std::memcmp(id.id, allzeros, sizeof(allzeros)) != 0;
}

/*dht::Peer*/
Peer::Peer(Ipv4 i, Port p, time_t n) noexcept
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

Peer::Peer(const Contact &c, time_t a, Peer *nxt) noexcept
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
    : Peer(0, 0, 0) {
}

bool
Peer::operator==(const Contact &c) const noexcept {
  return contact.operator==(c);
}

time_t
activity(const Node &head) noexcept {
  return std::max(head.request_activity, head.response_activity);
}

/*dht::Bucket*/
Bucket::Bucket()
    : contacts() {
}

Bucket::~Bucket() {
}

/*dht::RoutingTable*/
RoutingTable::RoutingTable(RoutingTable *h, RoutingTable *l)
    : type(NodeType::NODE) {
  node.higher = h;
  node.lower = l;
}

RoutingTable::RoutingTable()
    : bucket()
    , type(NodeType::LEAF) {
}

RoutingTable::~RoutingTable() {
  // DHT dht;
  if (type == NodeType::LEAF) {
    bucket.~Bucket();
  } else {
    // dealloc(dht, lower);
    // dealloc(dht, higher);
  }
}

/*dht::KeyValue*/
KeyValue::KeyValue(const Infohash &pid, KeyValue *nxt)
    : next(nxt)
    , peers(nullptr)
    , id() {
  std::memcpy(id.id, pid.id, sizeof(id.id));
}

/*dht::DHT*/
DHT::DHT(fd &udp, const ExternalIp &i)
    // self {{{
    : id()
    , client(udp)
    , ip(i)
    //}}}
    // peer-lookup db {{{
    , lookup_table(nullptr)
    , tokens()
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
    , contact_list()
    , value_list()
    // }}}
    // {{{
    /* sequence number of request used in transaction id gen?*/
    , sequence(0)
    /*timestamp of received request&response*/
    , last_activity(0)
    /*total nodes present intthe routing table*/
    , total_nodes(0)
    , now(0)
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
    , remote{p_remote} {
}

} // namespace dht
