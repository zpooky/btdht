#include "bootstrap.h"
#include <heap/binary.h>

namespace dht {
//==========================================
template <std::size_t sz>
static void
maybe_bootstrap_reset(DHTMetaBootstrap<sz> &self) noexcept {
  Config &cfg = self.config;
  Timestamp next = self.bootstrap_last_reset + cfg.bootstrap_reset;
  if ((self.now >= next) ||
      self.bootstrap_filter.unique_inserts >
          size_t((double)theoretical_max_capacity(self.bootstrap_filter) *
                 0.85)) {
    clear(self.bootstrap_filter);
    self.bootstrap_last_reset = self.now;
  }
}

template <std::size_t sz>
static void
__bootstrap_insert(DHTMetaBootstrap<sz> &self,
                   heap::MaxBinary<KContact> &contacts,
                   const KContact &remote) noexcept {
  assertx(remote.contact.port != 0);

  maybe_bootstrap_reset(self);

  if (!test(self.bootstrap_filter, remote.contact.ip)) {
    if (insert_eager(contacts, remote)) {
      insert(self.bootstrap_filter, remote.contact.ip);
    }
  }
}

void
bootstrap_insert(DHT &self, const IdContact &contact) noexcept {
  __bootstrap_insert(self.bootstrap_meta, self.bootstrap,
                     dht::KContact(contact, self.id));
}

void
bootstrap_insert(DHT &self, const Contact &contact) noexcept {
  __bootstrap_insert(self.bootstrap_meta, self.bootstrap,
                     dht::KContact(0, contact));
}

void
bootstrap_insert(DHTMetaScrape &self, const IdContact &contact) noexcept {
  __bootstrap_insert(self.bootstrap_filter, self.bootstrap,
                     dht::KContact(contact, self.routing_table.id));
}

void
bootstrap_insert(DHTMetaScrape &self, const Contact &contact) noexcept {
  __bootstrap_insert(self.bootstrap_filter, self.bootstrap,
                     dht::KContact(0, contact));
}

//==========================================
void
bootstrap_insert_force(DHT &self, KContact &remote) noexcept {
#if 0
  assertx(remote.contact.port != 0);
  if (remote.common > 0) {
    remote.common--;
  }
  maybe_bootstrap_reset(self);

  if (insert_eager(self.bootstrap, remote)) {
    insert(self.bootstrap_filter, remote.contact.ip);
  }
#endif
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
