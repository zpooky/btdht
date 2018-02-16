#ifndef SP_MAINLINE_DHT_BENCODE_OFFSET_H
#define SP_MAINLINE_DHT_BENCODE_OFFSET_H

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
peers(sp::Buffer &, const char *, sp::list<Contact> &) noexcept;

} // namespace d
} // namespace bencode
#endif
