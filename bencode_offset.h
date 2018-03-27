#ifndef SP_MAINLINE_DHT_BENCODE_OFFSET_H
#define SP_MAINLINE_DHT_BENCODE_OFFSET_H

#include "util.h"
#include <collection/Array.h>

namespace bencode {
namespace d {
/*
 * Consumed the buffer and validates that the underlieing content is valid
 * bencode.
 */
bool
dict_wildcard(sp::Buffer &) noexcept;

bool
nodes(sp::Buffer &, const char *, sp::list<dht::Node> &) noexcept;

bool
nodes(sp::Buffer &, const char *, sp::UinStaticArray<dht::Node, 256> &) noexcept;

bool
peers(sp::Buffer &, const char *, sp::list<Contact> &) noexcept;

bool
peers(sp::Buffer &, const char *, sp::UinStaticArray<Contact, 256> &) noexcept;

} // namespace d
} // namespace bencode
#endif
