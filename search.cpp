#include "search.h"

namespace dht {
//=====================================
Search *
search_find(dht::DHT &dht, SearchContext *needle) noexcept {
  sp::LinkedList<Search> &ctx = dht.searches;
  assertx(needle);
  return find_first(ctx, [&](const Search &current) {
    /**/
    return current.ctx == needle;
  });
}

//=====================================
void
search_decrement(SearchContext *ctx) noexcept {
  ctx->ref_cnt--;
  if (ctx->is_dead) {
    if (ctx->ref_cnt == 0) {
      delete ctx;
    }
  }
}

//=====================================
void
search_insert(Search &search, const dht::Node &contact) noexcept {
  /*test bloomfilter*/
  if (!test(search.searched, contact.id)) {
    /*insert into bloomfilter*/
    bool ires = insert(search.searched, contact.id);
    assertx(ires);
    insert_eager(search.queue, dht::KContact(contact, search.search.id));
  }
}

//=====================================
} // namespace dht
