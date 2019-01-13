#ifndef SP_MAINLINE_DHT_TEST_BENCODE_PRINT_H
#define SP_MAINLINE_DHT_TEST_BENCODE_PRINT_H

#include <cstdint>
#include "util.h"

template <typename Buffer>
void
bencode_print(Buffer &) noexcept;

template <typename Buffer>
bool
find_entry(Buffer &, const char *key, /*OUT*/ sp::byte *val,
           /*IN/OUT*/ std::size_t &) noexcept;

#endif
