#ifndef SP_MAINLINE_DHT_PRIV_KRPC_H
#define SP_MAINLINE_DHT_PRIV_KRPC_H

#include "decode_bencode.h"
#include "shared.h"
#include "util.h"

namespace krpc {
namespace priv {
//=====================================
namespace request {
bool
dump(sp::Buffer &b, const krpc::Transaction &) noexcept;

bool
dump_scrape(sp::Buffer &b, const krpc::Transaction &) noexcept;

bool
dump_db(sp::Buffer &b, const krpc::Transaction &) noexcept;

bool
statistics(sp::Buffer &b, const krpc::Transaction &t) noexcept;

bool
search(sp::Buffer &b, const krpc::Transaction &, const dht::Infohash &,
       std::size_t) noexcept;

bool
stop_search(sp::Buffer &b, const krpc::Transaction &,
            const dht::Infohash &) noexcept;
} // namespace request

//=====================================
namespace response {
bool
dump(sp::Buffer &b, const krpc::Transaction &t, const dht::DHT &) noexcept;

bool
dump_scrape(sp::Buffer &b, const krpc::Transaction &t,
            const dht::DHT &) noexcept;

bool
dump_db(sp::Buffer &b, const krpc::Transaction &t, const dht::DHT &) noexcept;

bool
statistics(sp::Buffer &b, const krpc::Transaction &t,
           const dht::Stat &) noexcept;

bool
search(sp::Buffer &b, const krpc::Transaction &t) noexcept;

bool
search_stop(sp::Buffer &b, const krpc::Transaction &t) noexcept;

bool
announce_this(sp::Buffer &b, const krpc::Transaction &t) noexcept;
} // namespace response

//=====================================
namespace event {
template <typename Contacts>
bool
found(sp::Buffer &, const dht::Infohash &, const Contacts &) noexcept;

} // namespace event

//=====================================
} // namespace priv
} // namespace krpc

#endif
