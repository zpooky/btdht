#ifndef SP_MAINLINE_DHT_DSL_BENCODE_H
#define SP_MAINLINE_DHT_DSL_BENCODE_H

#include "bencode.h"
#include "dstack.h"
#include "shared.h"

namespace bencode {
//=====================================
namespace e {
// bool
// value_raw(sp::Buffer &b, const Contact &p) noexcept;

// bool
// pair(sp::Buffer &buf, const char *key, const Contact &p) noexcept;

bool
pair_compact(sp::Buffer &, const char *, const Contact *) noexcept;

bool
pair_compact(sp::Buffer &buf, const char *key,
             const sp::UinArray<Contact> &list) noexcept;

bool
pair_compact(sp::Buffer &, const char *, const sp::list<dht::Node> &) noexcept;

bool
pair_compact(sp::Buffer &, const char *, const dht::Node **,
             std::size_t) noexcept;

} // namespace e

//=====================================
namespace e {
namespace priv {
bool
value(sp::Buffer &buf, const Contact &) noexcept;

bool
value(sp::Buffer &buf, const dht::Node &) noexcept;

bool
value(sp::Buffer &buf, const dht::Bucket &t) noexcept;

bool
value(sp::Buffer &buf, const dht::RoutingTable &t) noexcept;

bool
pair(sp::Buffer &buf, const char *key, const dht::StatTrafic &t) noexcept;

bool
pair(sp::Buffer &buf, const char *key, const dht::StatDirection &d) noexcept;

bool
value(sp::Buffer &buf, const sp::list<Contact> &) noexcept;

bool
value(sp::Buffer &buf, const sp::dstack<Contact> &) noexcept;

bool
pair(sp::Buffer &buf, const char *key, const sp::list<Contact> &t) noexcept;

bool
pair(sp::Buffer &buf, const char *key, const sp::dstack<Contact> &) noexcept;

bool
pair(sp::Buffer &buf, const char *key,
     const heap::MaxBinary<dht::KContact> &) noexcept;
} // namespace priv
} // namespace e

//=====================================
namespace d {
namespace priv {
bool
value(sp::Buffer &buf, Contact &) noexcept;

bool
value(sp::Buffer &buf, sp::UinArray<Contact> &) noexcept;
} // namespace priv
} // namespace d

//=====================================
} // namespace bencode

#endif
