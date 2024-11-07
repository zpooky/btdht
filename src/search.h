#ifndef SP_MAINLINE_DHT_SEARCH_H
#define SP_MAINLINE_DHT_SEARCH_H

#include <heap/binary.h>
#include <tree/StaticTree.h>
#include <tree/avl.h>
#include <util/Bloomfilter.h>

#include "util.h"

namespace dht {
// ========================================
struct SearchContext : public get_peers_context {
  // ref_cnt is ok since we are in a single threaded ctx
  // incremented for each successfull client::get_peers
  // decremented on receieve & on timeout
  std::size_t ref_cnt;
  // whether this instance should be reclaimd. if ref_cnt is 0
  const Infohash search;
  bool is_dead;

  explicit SearchContext(const Infohash &) noexcept;
};

// ========================================
struct Search {
  SearchContext *ctx;
  Infohash search;

  sp::StaticArray<sp::hasher<NodeId>, 2> hashers;
  sp::BloomFilter<NodeId, 8 * 1024 * 1024> searched;
  sp::Timestamp timeout;

  heap::StaticMaxBinary<KContact, 1024> queue;
  sp::LinkedList<Contact> result;

  Search *next;
  Search *priv;

  std::size_t fail;

  explicit Search(const Infohash &) noexcept;

#if 0
  Search(Search &&) noexcept;
#endif

  Search(const Search &&) = delete;
  Search(const Search &) = delete;

  Search &
  operator=(const Search &) = delete;
  Search &
  operator=(const Search &&) = delete;

  ~Search() noexcept;
};

// ========================================
bool
operator>(const Infohash &, const Search &) noexcept;

bool
operator>(const Search &, const Search &) noexcept;

bool
operator>(const Search &, const Infohash &) noexcept;

//=====================================
struct DHTMetaSearch {
  Search *search_root;
  avl::Tree<Search> searches;
  DHTMetaSearch();

  ~DHTMetaSearch() {
  }
};

//=====================================
Search *
search_find(DHTMetaSearch &, SearchContext *) noexcept;

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
