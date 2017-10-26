#ifndef SP_MAINLINE_DHT_BENCODE_H
#define SP_MAINLINE_DHT_BENCODE_H

#include "shared.h"
#include <cstdint>

namespace bencode {

bool
encode(sp::Buffer &, std::uint32_t) noexcept;
bool
encode(sp::Buffer &, std::int32_t) noexcept;

bool
encode(sp::Buffer &, const char *) noexcept;
bool
encode(sp::Buffer &, const char *, std::size_t) noexcept;
template <std::size_t SIZE>
bool
encode(sp::Buffer &, const char (&)[SIZE]) noexcept;
template <std::size_t SIZE>
bool
encode(sp::Buffer &, const char (&)[SIZE], std::size_t) noexcept;

bool
encodeList(sp::Buffer &, bool (*)(sp::Buffer &)) noexcept;

bool
encodeDict(sp::Buffer &, bool (*)(sp::Buffer &)) noexcept;

//----------------------------------
bool
decode(sp::Buffer &, std::uint32_t &) noexcept;

bool
decode(sp::Buffer &, std::int32_t &) noexcept;

} // namespace bencode

#endif
