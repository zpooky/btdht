#ifndef SP_MAINLINE_DHT_SEARCH_H
#define SP_MAINLINE_DHT_SEARCH_H

#include "shared.h"

namespace dht {
//=====================================
Search *
search_find(dht::DHT &, SearchContext *) noexcept;

//=====================================
void
search_decrement(SearchContext *ctx) noexcept;

//=====================================
void
search_insert(Search &search, const dht::Node &contact) noexcept;

//=====================================
} // namespace dht

#endif
