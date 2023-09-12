#include "search.h"
#include <hash/crc.h>
#include <hash/djb2.h>
#include <hash/fnv.h>
#include <hash/standard.h>

namespace dht {
//=====================================
SearchContext::SearchContext(const Infohash &s) noexcept
    : ref_cnt(1)
    , search(s)
    , is_dead(false) {
}

//=====================================
Search::Search(const Infohash &s) noexcept
    : ctx(new SearchContext(s))
    , search(s)
    , hashers()
    , searched(hashers)
    , timeout(0)
    , queue()
    , result()
    , next(nullptr)
    , priv(nullptr)
    , fail(0) {

  auto djb_f = [](const NodeId &id) -> std::size_t {
    return djb2::encode32(id.id, sizeof(id.id));
  };

  auto fnv_f = [](const NodeId &id) -> std::size_t {
    return fnv_1a::encode64(id.id, sizeof(id.id));
  };

  assertx_n(insert(hashers, djb_f));
  assertx_n(insert(hashers, fnv_f));
}

#if 0
Search::Search(Search &&o) noexcept
    : ctx(nullptr)
    , search()
    , remote()
    , hashers()
    , searched()
    , timeout()
    , queue()
    , result()
    , search_next(nullptr) {

  using std::swap;
  swap(ctx, o.ctx);
  swap(search, o.search);
  swap(remote, o.remote);
  swap(hashers, o.hashers);
  swap(searched, o.searched);
  swap(timeout, o.timeout);
  swap(queue, o.queue);
  swap(result, o.result);
  swap(search_next, o.search_next);
}
#endif

Search::~Search() noexcept {
  if (ctx) {
    ctx->is_dead = true;
    search_decrement(ctx);
  }
}

// ========================================
bool
operator>(const Infohash &f, const Search &o) noexcept {
  return f > o.search;
}

bool
operator>(const Search &f, const Search &s) noexcept {
  return f.search > s.search;
}

bool
operator>(const Search &f, const Infohash &s) noexcept {
  return f.search > s;
}

//=====================================
DHTMetaSearch::DHTMetaSearch()
    : search_root(nullptr)
    , searches() {
}

//=====================================
#if 0
Search::Search(Search &&o) noexcept
    : ctx(nullptr)
    , search(o.search)
    , remote(o.remote)
    , hashers()
    , searched(hashers)
    , timeout(0)
    , raw_queue(nullptr)
    , queue(nullptr, 0) {
  using sp::swap;

  swap(ctx, o.ctx);
  insert_all(hashers, o.hashers);
  swap(searched, o.searched);
  swap(timeout, o.timeout);
}
#endif

//=====================================
Search *
search_find(DHTMetaSearch &self, SearchContext *needle) noexcept {
  assertx(needle);
  assertx(needle);
  return find(self.searches, needle->search);
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
