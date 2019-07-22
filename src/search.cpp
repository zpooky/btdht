#include "search.h"

namespace dht {
//=====================================
Search *
search_find(DHT &dht, SearchContext *needle) noexcept {
  assertx(needle);
  assertx(needle);
  return find(dht.searches, needle->search);
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
search_increment(SearchContext *ctx) noexcept {
  assertx(ctx->ref_cnt > 0);
  ctx->ref_cnt++;
}

//=====================================
void
search_insert(Search &self, const IdContact &contact) noexcept {
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
