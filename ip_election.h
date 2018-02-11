#ifndef SP_MAINLINE_DHT_IP_VOTE_H
#define SP_MAINLINE_DHT_IP_VOTE_H

#include "util.h"
#include <collection/Array.h>
#include <tuple>
#include <util/Bloomfilter.h>

namespace sp {

struct ip_election {
  using entry = std::tuple<Contact, std::size_t>;

  StaticArray<entry, 16> table;
  StaticArray<Hasher<Ip>, 2> hs;
  BloomFilter<Ip, 256> voted;
  std::size_t votes;

  ip_election() noexcept;
};

bool
vote(ip_election &, const Contact &, const Contact &) noexcept;

const Contact *
winner(const ip_election &, std::size_t) noexcept;

void
print_result(const ip_election &) noexcept;

} // namespace sp

#endif
