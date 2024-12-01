#ifndef SP_MAINLINE_DHT_ROUTING_TABLE_H
#define SP_MAINLINE_DHT_ROUTING_TABLE_H

#include <cstddef>
#include <cstdint>
#include <limits.h>

#include <heap/binary.h>
#include <prng/xorshift.h>

#include "dstack.h"
#include "timeout.h"
#include "util.h"

namespace dht {
//=====================================
struct Bucket {
  static constexpr std::size_t K = 32;
  Node contacts[K];
  std::size_t length;

  Bucket() noexcept;
  ~Bucket() noexcept;

  Bucket(const Bucket &) = delete;
  Bucket(const Bucket &&) = delete;

  Bucket &
  operator=(const Bucket &) = delete;
  Bucket &
  operator=(const Bucket &&) = delete;
};

template <typename F>
bool
for_all(Bucket &b, F f) noexcept;

template <typename F>
bool
for_all(const Bucket &b, F f) noexcept;

template <typename F>
void
for_each(Bucket &b, F f) noexcept;

template <typename F>
void
for_each(const Bucket &b, F f) noexcept;

//=====================================
struct RoutingTable;

struct RoutingTable {
  ssize_t depth;
  RoutingTable *in_tree;
  Bucket bucket;
  RoutingTable *parallel;

  RoutingTable(ssize_t) noexcept;

  RoutingTable(const RoutingTable &) = delete;
  RoutingTable(const RoutingTable &&) = delete;

  RoutingTable &
  operator=(const RoutingTable &) = delete;
  RoutingTable &
  operator=(const RoutingTable &&) = delete;

  ~RoutingTable() noexcept;
};

bool
is_empty(const RoutingTable &) noexcept;

#if 0
bool
operator<(const RoutingTable &, std::size_t) noexcept;

bool
operator<(const RoutingTable &, const RoutingTable &) noexcept;
#endif

struct RoutingTableLess {
  bool
  operator()(const RoutingTable *f, std::size_t s) const noexcept;

  bool
  operator()(const RoutingTable *f, const RoutingTable *s) const noexcept;
};

template <typename F>
bool
for_all(const RoutingTable *it, F f) noexcept;

template <typename F>
bool
for_all_node(const RoutingTable *it, F f) noexcept;

//=====================================
struct DHTMetaRoutingTable {
  RoutingTable *root;
  heap::Binary<RoutingTable *, RoutingTableLess> rt_reuse;

  prng::xorshift32 &random;
  timeout::TimeoutBox &tb;
  const dht::NodeId &id;
  const Timestamp &now;
  const dht::Config &config;

  std::uint32_t total_nodes;
  std::uint32_t bad_nodes;

  sp::UinStaticArray<
      std::tuple<void (*)(void *ctx, const Node &) noexcept, void *>, 4>
      retire_good;
  void *cache;

  DHTMetaRoutingTable(std::size_t capacity, prng::xorshift32 &,
                      timeout::TimeoutBox &tb, Timestamp &, const dht::NodeId &,
                      const dht::Config &);
  ~DHTMetaRoutingTable();
};

void
debug_for_each(const DHTMetaRoutingTable &, void *,
               void (*)(void *, const DHTMetaRoutingTable &,
                        const RoutingTable &, const Node &)) noexcept;

std::size_t
debug_levels(const DHTMetaRoutingTable &) noexcept;

void
debug_print(const char *ctx, const DHTMetaRoutingTable &self) noexcept;

bool
debug_assert_all(const DHTMetaRoutingTable &);

bool
debug_timeout_unlink_reset(DHTMetaRoutingTable &self, Node &contact);

bool
is_good(const DHTMetaRoutingTable &dht, const Node &contact) noexcept;

/**/
void
multiple_closest(DHTMetaRoutingTable &, const NodeId &, Node **result,
                 std::size_t) noexcept;

template <std::size_t SIZE>
void
multiple_closest(DHTMetaRoutingTable &self, const NodeId &id,
                 Node *(&result)[SIZE]) noexcept {
  return multiple_closest(self, id, result, SIZE);
}

void
multiple_closest(DHTMetaRoutingTable &, const Key &, Node **,
                 std::size_t) noexcept;

template <std::size_t SIZE>
void
multiple_closest(DHTMetaRoutingTable &self, const Key &id,
                 Node *(&result)[SIZE]) noexcept {
  return multiple_closest(self, id, result, SIZE);
}

void
multiple_closest(DHTMetaRoutingTable &, const Infohash &, Node **,
                 std::size_t) noexcept;

template <std::size_t SIZE>
void
multiple_closest(DHTMetaRoutingTable &self, const Infohash &id,
                 Node *(&result)[SIZE]) noexcept {
  return multiple_closest(self, id, result, SIZE);
}

// bool
// valid(DHTMetaRoutingTable &, const krpc::Transaction &) noexcept;

Node *
find_node(DHTMetaRoutingTable &, const NodeId &) noexcept;

const Bucket *
bucket_for(DHTMetaRoutingTable &, const NodeId &) noexcept;

Node *
insert(DHTMetaRoutingTable &, const Node &) noexcept;

std::uint32_t
max_routing_nodes(const DHTMetaRoutingTable &) noexcept;

std::uint32_t
nodes_good(const DHTMetaRoutingTable &) noexcept;

std::uint32_t
nodes_total(const DHTMetaRoutingTable &) noexcept;

std::uint32_t
nodes_bad(const DHTMetaRoutingTable &) noexcept;

//=====================================
//=====================================
//=====================================
template <typename F>
bool
for_all(Bucket &b, F f) noexcept {
  bool result = true;
  for (std::size_t i = 0; i < Bucket::K && result; ++i) {
    if (is_valid(b.contacts[i])) {
      result = f(b.contacts[i]);
    }
  }
  return result;
}

template <typename F>
bool
for_all(const Bucket &b, F f) noexcept {
  bool result = true;
  for (std::size_t i = 0; i < Bucket::K && result; ++i) {
    const Node &current = b.contacts[i];
    if (is_valid(current)) {
      result = f(current);
    }
  }
  return result;
}

template <typename F>
void
for_each(Bucket &b, F f) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    if (is_valid(b.contacts[i])) {
      f(b.contacts[i]);
    }
  }
}

template <typename F>
void
for_each(const Bucket &b, F f) noexcept {
  for (std::size_t i = 0; i < Bucket::K; ++i) {
    const auto &current = b.contacts[i];
    if (is_valid(current)) {
      f(current);
    }
  }
}

// ========================================
template <typename F>
bool
for_all(const RoutingTable *it, F f) noexcept {
  while (it) {

    auto it_width = it;
    while (it_width) {

      const RoutingTable &current = *it_width;
      if (!f(current)) {
        return false;
      }
      it_width = it_width->parallel;
    } // while

    it = it->in_tree;
  } // while

  return true;
}

template <typename F>
bool
for_all_node(const RoutingTable *it, F f) noexcept {
  return for_all(it, [&f](const RoutingTable &r) {
    /**/
    return for_all(r.bucket, f);
  });
}

// ========================================

} // namespace dht

#endif
