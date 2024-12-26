#include "shared.h"

#include <algorithm>
#include <cstring>
#include <util/assert.h>
#include <utility>

#include <hash/djb2.h>
#include <hash/fnv.h>

#include "scrape.h"

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
TxContext::timeout(dht::DHT &dht, Tx *tx) noexcept {
  assertx(tx);
  if (int_timeout) {
    krpc::Transaction t(tx->prefix, tx->suffix);
    int_timeout(dht, t, tx->sent, closure);
  }
}

bool
TxContext::handle(dht::MessageContext &ctx) noexcept {
  assertx(int_handle);
  return int_handle(ctx, closure);
}

TxContext::TxContext(TxHandle h, TxCancelHandle ch, void *c) noexcept
    : int_handle(h)
    , int_timeout(ch)
    , closure(c)
    , latency(0) {
  assertx(h);
  assertx(ch);
}

TxContext::TxContext() noexcept
    : int_handle(nullptr)
    , int_timeout(nullptr)
    , closure(nullptr)
    , latency(0) {
}

void
reset(TxContext &ctx) noexcept {
  ctx.int_handle = nullptr;
  ctx.int_timeout = nullptr;
  ctx.closure = nullptr;
}

/*dht::Tx*/
Tx::Tx() noexcept
    : context{}
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

//=====================================
namespace dht {
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
    , sample_infohashes(0)
    , error(0) {
}

StatDirection::StatDirection() noexcept
    : request()
    , response_timeout()
    , response()
    , parse_error(0) {
}

Stat::Stat() noexcept
    : transmit()
    , received()
    , known_tx()
    , unknown_tx()
    , scrape_swapped_ih() {
}

DHTMetaScrape::DHTMetaScrape(dht::DHT &self, const dht::NodeId &_ih) noexcept

    : dht{self}
    , id{_ih}
    , tb{self.now}
    , routing_table{30, self.random, tb, self.now, id, self.config}
    , bootstrap{}
    , started{self.now}
    , now{self.now}
    , bootstrap_filter(self.scrape_bootstrap_filter)
    , upcoming_sample_infohashes(0)
    , box(self.now) { //
}

// dht::DHT
DHT::DHT(const Contact &self, Client &_client, prng::xorshift32 &r,
         Timestamp &n, const dht::Options &options, sp_upnp *_upnp) noexcept

    // self {{{
    : id()
    , client{_client}
    , log()
    , external_ip(self)
    , random(r)
    , election()
    , statistics()
    , ip_cnt(0)
    , config()
    , core()
    , should_exit(false)
    //}}}
    , db{config, r, n, options}
    , routing_table(100, r, this->tb, n, this->id, config)
    , tb(n)
    //}}}
    // recycle contact list {{{
    , recycle_contact_list()
    // }}}
    // {{{
    /*timestamp of received request&response*/
    , last_activity(0)
    /*total nodes present in the routing table*/
    , now(n)
    // bootstrap {{{
    , ip_hashers()
    , bootstrap_meta{config, ip_hashers, now}
    , bootstrap()
    // }}}
    , searches()
    // {{{
    , active_scrapes()
    , scrape_hour{}
    , scrape_hour_idx(0)
    , scrape_hour_time(n)
    , scrape_bootstrap_filter(config, ip_hashers, now)
    , scrape_active_sample_infhohash(0)
    , scrape_retire_good()
    , scrape_backoff(false)
    // }}}
    , upnp_expiry{n}
    , upnp{_upnp}
    , upnp_external_port{0} {

  for (size_t i = 0; i < capacity(scrape_hour); ++i) {
    assertx_n(emplace(scrape_hour, ip_hashers));
  }
  assertx(is_full(scrape_hour));

  assertx_n(insert(ip_hashers, djb_ip));
  assertx_n(insert(ip_hashers, fnv_ip));

  for (size_t i = 0; i < 2; ++i) {
    fill(r, &this->db.key[i].key, sizeof(this->db.key[i].key));
    this->db.key[i].created = now;
  }

  emplace(routing_table.retire_good, scrape::main_on_retire_good, (void *)this);
}

DHT::~DHT() {
  // TODO reclaim
}

//=====================================
/*dht::MessageContext*/
MessageContext::MessageContext(DHT &p_dht, krpc::ParseContext &ctx,
                               sp::Buffer &p_out, Contact p_remote) noexcept
    : domain(ctx.domain)
    , query(ctx.query)
    , dht{p_dht}
    , in{ctx.decoder}
    , out{p_out}
    , transaction{ctx.tx}
    , remote{p_remote}
    , read_only{ctx.read_only}
    , pctx{ctx}
    , sample_infohashes{fopen("./sample_infohashes.log", "a")} {
}

MessageContext::~MessageContext() {
  if (sample_infohashes) {
    fclose(sample_infohashes);
    sample_infohashes = nullptr;
  }
}

//=====================================
} // namespace dht
