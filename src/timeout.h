#ifndef SP_MAINLINE_DHT_TIMEOUT_H
#define SP_MAINLINE_DHT_TIMEOUT_H

#include "db.h"
#include "util.h"

namespace timeout {
struct Timeout {
  Timestamp timeout_next;
  dht::Node *timeout_node;
  Timestamp &now;

  explicit Timeout(Timestamp &now) noexcept;
  Timeout(const Timeout &) = delete;
};

//=====================================
std::size_t
debug_count_nodes(const Timeout &) noexcept;

//=====================================
const dht::Node *
debug_find_node(const Timeout &, const dht::Node *) noexcept;

//=====================================
void
unlink(dht::Node *&head, dht::Node *) noexcept;

void
unlink(Timeout &, dht::Node *) noexcept;

void
unlink(dht::KeyValue &, dht::Peer *) noexcept;

//=====================================
void
append_all(Timeout &, dht::Node *) noexcept;

void
append_all(dht::KeyValue &, dht::Peer *) noexcept;

//=====================================
void
prepend(Timeout &, dht::Node *) noexcept;

//=====================================
void
insert_new(Timeout &, dht::Node *) noexcept;

//=====================================
void
insert(dht::Peer *priv, dht::Peer *subject, dht::Peer *next) noexcept;

//=====================================
dht::Node *
take_node(Timeout &, sp::Milliseconds timeout) noexcept;

dht::Peer *
take_peer(db::DHTMetaDatabase &dht, dht::KeyValue &,
          sp::Milliseconds timeout) noexcept;

// TODO XXX what is timeout????!?!?! (i want last sent timeout)

//=====================================
} // namespace timeout

#endif
