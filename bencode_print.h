#ifndef SP_MAINLINE_DHT_TEST_BENCODE_PRINT_H
#define SP_MAINLINE_DHT_TEST_BENCODE_PRINT_H

#include "bencode.h"

namespace sp {
void
bencode_print(sp::Buffer &) noexcept;

bool
find_entry(Buffer &, const char *key, /*OUT*/ byte *val,
           /*IN/OUT*/ std::size_t &) noexcept;

} // namespace sp

#endif
