#include "bootstrap.h"

namespace dht {
//==========================================
void
bootstrap_insert(DHT &self, const KContact &remote) noexcept {
  assertx(remote.contact.port != 0);

  if (!test(self.bootstrap_filter, remote.contact.ip)) {
    if (insert_eager(self.bootstrap, remote)) {
      insert(self.bootstrap_filter, remote.contact.ip);
    }
  }
}

void
bootstrap_insert(DHT &self, const IdContact &contact) noexcept {
  bootstrap_insert(self, dht::KContact(contact, self.id));
}

//==========================================
void
bootstrap_insert_force(DHT &self, KContact &remote) noexcept {
  assertx(remote.contact.port != 0);
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
  clear(self.bootstrap_filter);
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
bool
bootstrap_take_head(DHT &self, dht::KContact &out) noexcept {
  if (is_empty(self.bootstrap)) {
    self.topup_bootstrap(self);
  }
  return take_head(self.bootstrap, out);
}

} // namespace dht
