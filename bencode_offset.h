#ifndef SP_MAINLINE_DHT_BENCODE_OFFSET_H
#define SP_MAINLINE_DHT_BENCODE_OFFSET_H

namespace bencode {
namespace d {
/*
 * Consumed the buffer and validates that the underlieing content is valid
 * bencode.
 */
bool
dict_wildcard(bencode::d::Decoder &d) noexcept;

bool
nodes(bencode::d::Decoder &d, const char *, sp::list<dht::Node> &) noexcept;

bool
peers(bencode::d::Decoder &d, const char *, sp::list<dht::Contact> &) noexcept;

} // namespace d
} // namespace bencode
#endif
