#ifndef SP_MAINLINE_DHT_SEARCH_H
#define SP_MAINLINE_DHT_SEARCH_H

#include "shared.h"

namespace dht {
//=====================================
Search *
search_find(DHT &, SearchContext *) noexcept;

//=====================================
void
search_decrement(SearchContext *) noexcept;

//=====================================
void
search_increment(SearchContext *) noexcept;

//=====================================
void
search_insert(Search &, const IdContact &) noexcept;

//=====================================
void
search_insert_result(Search &, const Contact &) noexcept;

//=====================================
} // namespace dht

#endif
