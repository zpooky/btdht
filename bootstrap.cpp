#include "bootstrap.h"

namespace dht {
//==========================================
void
bootstrap_insert(DHT &self, const KContact &remote) noexcept {
  if (!test(self.bootstrap_filter, remote.contact.ip)) {
    if (insert_eager(self.bootstrap, remote)) {
      insert(self.bootstrap_filter, remote.contact.ip);
    }
  }
}

void
bootstrap_insert(DHT &self, const Node &node) noexcept {
  bootstrap_insert(self, dht::KContact(node, self.id));
}

//==========================================
void
bootstrap_insert_force(DHT &self, KContact &remote) noexcept {
  if (remote.common > 0) {
    remote.common--;
  }

  if (insert_eager(self.bootstrap, remote)) {
    insert(self.bootstrap_filter, remote.contact.ip);
  }
}

//==========================================
void
bootstrap_reset(DHT &self) noexcept {
  auto &filter = self.bootstrap_filter;
  auto &set = filter.bitset;
  std::memset(set.raw, 0, sizeof(set.raw));
}

//==========================================
void
bootstrap_reclaim(DHT &, dht::KContact *in) noexcept {
  assertx(in);
  delete in;
}

//==========================================
dht::KContact *
bootstrap_alloc(DHT &, const dht::KContact &cur) noexcept {
  return new dht::KContact(cur);
}

//==========================================
} // namespace dht
