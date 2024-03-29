#include "ip_election.h"
#include <hash/djb2.h>
#include <hash/fnv.h>
#include <util/assert.h>

namespace sp {
ip_election::ip_election() noexcept
    : table()
    , hs()
    , voted(hs)
    , votes(0) {

  auto djb = [](const Ip &ip) -> std::size_t {
    if (ip.type == IpType::IPV6) {
#ifdef IP_IPV6
      return djb2a::encode32(&ip.ipv6, sizeof(ip.ipv6));
#else
      assertx(false);
#endif
    } else {
      return djb2a::encode32(&ip.ipv4, sizeof(ip.ipv4));
    }
  };

  auto fnv1a = [](const Ip &ip) -> std::size_t {
    if (ip.type == IpType::IPV6) {
#ifdef IP_IPV6
      return fnv_1a::encode64(&ip.ipv6, sizeof(ip.ipv6));
#else
      assertx(false);
#endif
    } else {
      return fnv_1a::encode64(&ip.ipv4, sizeof(ip.ipv4));
    }
  };

  auto dins = insert(hs, djb);
  assertx(dins);
  auto fins = insert(hs, fnv1a);
  assertx(fins);
}

bool
vote(ip_election &ctx, const Contact &by, const Contact &vote_for) noexcept {
  ++ctx.votes;
  if (!test(ctx.voted, by.ip)) {
    insert(ctx.voted, by.ip);

    auto *present = find(ctx.table, [&](auto &c) { //
      return std::get<0>(c) == vote_for;
    });

    if (!present) {
      present = insert(ctx.table, std::make_tuple(vote_for, 0));
    }

    if (present) {
      std::size_t &it = std::get<1>(*present);
      ++it;

      return true;
    }
  }

  return false;
}

struct ContactEntry {
  const Contact *contact;
  std::size_t cnt;

  ContactEntry()
      : contact(nullptr)
      , cnt(0) {
  }
};

const Contact *
winner(const ip_election &ctx, std::size_t min) noexcept {
  ContactEntry in;

  auto *best = reduce(ctx.table, &in, [](auto *acum, const auto &current) {
    if (acum->cnt > 0) {
      const Contact &c = std::get<0>(*current);
      acum->contact = &c;
      acum->cnt = std::get<1>(*current);
    }

    return acum;
  });

  assertx(best);

  if (best->cnt >= min) {
    return best->contact;
  }

  return nullptr;
}

void
print_result(const ip_election &election) noexcept {
  printf("# election(invocations %zu)\n", election.votes);
  for_each(election.table, [](const auto &e) {
    Contact c = std::get<0>(e);
    std::size_t votes = std::get<1>(e);
    char str[64] = {0};
    if (!to_string(c, str)) {
      assertx(false);
    }
    printf("%s - %zu\n", str, votes);
  });
}

} // namespace sp
