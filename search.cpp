#include "search.h"

namespace dht {
//=====================================
Search *
search_find(DHT &dht, SearchContext *needle) noexcept {
  assertx(needle);
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
search_insert(Search &self, const Node &contact) noexcept {
  /*test bloomfilter*/
  if (!test(self.searched, contact.id)) {
    /*insert into bloomfilter*/
    bool ires = insert(self.searched, contact.id);
    assertx(ires);
    insert_eager(self.queue, KContact(contact, self.search.id));
  }
}

//=====================================
void
search_insert_result(Search &self, const Contact &peer) noexcept {
  insert(self.result, peer);
}

//=====================================
} // namespace dht
