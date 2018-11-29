#include "timeout.h"
#include <util/assert.h>

namespace timeout {
// template <typename T>
// static T *
// last(T *node) noexcept {
// Lstart:
//   if (node) {
//     if (node->timeout_next) {
//       node = node->timeout_next;
//       goto Lstart;
//     }
//   }
//   return node;
// } // timeout::last()

template <typename T>
void
internal_unlink(T *&head, T *const node) noexcept {
  T *priv = node->timeout_priv;
  T *next = node->timeout_next;

  assertx(priv);
  assertx(next);

  if (priv == node || next == node) {
    assertx(priv == node);
    assertx(next == node);
    priv = nullptr;
    next = nullptr;
  }

  if (priv) {
    priv->timeout_next = next;
  }

  if (next) {
    next->timeout_priv = priv;
  }

  if (head == node) {
    head = next;
  }

  node->timeout_next = nullptr;
  node->timeout_priv = nullptr;
}

void
unlink(dht::Node *&head, dht::Node *contact) noexcept {
  return internal_unlink(head, contact);
} // timeout::unlink()

void
unlink(dht::DHT &ctx, dht::Node *contact) noexcept {
  return unlink(ctx.timeout_node, contact);
} // timeout::unlink()

void
unlink(dht::Peer *&head, dht::Peer *peer) noexcept {
  return internal_unlink(head, peer);
} // timeout::unlink()

template <typename T>
void
internal_append_all(T *&head, T *const node) noexcept {
  assertx(node->timeout_next == nullptr);
  assertx(node->timeout_priv == nullptr);

  if (!head) {
    // T *const lst = last(node);

    // lst->timeout_next = node;
    node->timeout_priv = node;
    node->timeout_next = node;

    head = node;
  } else {
    // T *const l = last(node);

    T *const priv = head->timeout_priv;
    assertx(priv);

    priv->timeout_next = node;
    head->timeout_priv = node;

    node->timeout_priv = priv;
    node->timeout_next = head;
  }
}

void
append_all(dht::DHT &ctx, dht::Node *node) noexcept {
  return internal_append_all(ctx.timeout_node, node); //<--
} // timeout::append_all()

void
append_all(dht::DHT &ctx, dht::Peer *peer) noexcept {
  return internal_append_all(ctx.timeout_peer, peer);
} // timeout::append_all()

void
prepend(dht::DHT &dht, dht::Node *ret) noexcept {
  assertx(ret);
  assertx(ret->timeout_next == nullptr);
  assertx(ret->timeout_priv == nullptr);

  dht::Node *const head = dht.timeout_node;
  if (head) {
    dht::Node *priv = head->timeout_priv;
    assertx(priv);

    priv->timeout_next = ret;
    head->timeout_priv = ret;

    ret->timeout_priv = priv;
    ret->timeout_next = head;

    dht.timeout_node = ret;
  } else {
    dht.timeout_node = ret->timeout_priv = ret->timeout_next = ret;
  }
}

void
insert_new(dht::DHT &dht, dht::Node *ret) noexcept {
  return prepend(dht, ret);
}

} // namespace timeout
