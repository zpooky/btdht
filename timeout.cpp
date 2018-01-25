#include "timeout.h"

namespace timeout {
template <typename T>
static T *
last(T *node) noexcept {
Lstart:
  if (node) {
    if (node->timeout_next) {
      node = node->timeout_next;
      goto Lstart;
    }
  }
  return node;
} // timeout::last()

template <typename T>
void
internal_unlink(T *&head, T *const contact) noexcept {
  T *priv = contact->timeout_priv;
  T *next = contact->timeout_next;

  assert(priv);
  assert(next);

  if (priv == contact || next == contact) {
    assert(priv == contact);
    assert(next == contact);
    priv = nullptr;
    next = nullptr;
  }

  if (priv)
    priv->timeout_next = next;

  if (next)
    next->timeout_priv = priv;

  if (head == contact)
    head = next;

  contact->timeout_next = nullptr;
  contact->timeout_priv = nullptr;
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
  if (!head) {
    T *const lst = last(node);

    lst->timeout_next = node;
    node->timeout_priv = lst;
    node->timeout_next = node;

    head = node;
  } else {
    T *const l = last(node);

    T *const priv = head->timeout_priv;
    assert(priv);
    node->timeout_priv = priv;
    priv->timeout_next = node;

    T *const next = head;
    l->timeout_next = next;
    next->timeout_priv = l;
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

} // namespace timeout
