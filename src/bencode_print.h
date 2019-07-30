#ifndef SP_MAINLINE_DHT_TEST_BENCODE_PRINT_H
#define SP_MAINLINE_DHT_TEST_BENCODE_PRINT_H

#include "util.h"
#include <cstdint>
#include <cstdio>

//=====================================
void
bencode_print_out(FILE *) noexcept;

//=====================================
template <typename Buffer>
void
bencode_print(Buffer &) noexcept;

//=====================================
void
bencode_print_file(const char *file) noexcept;

//=====================================
template <typename Buffer>
bool
find_entry(Buffer &, const char *key, /*OUT*/ sp::byte *val,
           /*IN/OUT*/ std::size_t &) noexcept;

//=====================================
#endif
