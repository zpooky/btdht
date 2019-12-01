#include "timeout.h"
#include <util/assert.h>

namespace timeout {
//=====================================
std::size_t
debug_count_nodes(const dht::DHT &self) noexcept {
  std::size_t result = 0;
  auto it = self.timeout_node;

  dht::Node *const head = it;
  if (head) {
    do {
      ++result;
      it = it->timeout_next;
    } while (it != head);
  }

  return result;
}

//=====================================
const dht::Node *
debug_find_node(const dht::DHT &self, const dht::Node *needle) noexcept {
  dht::Node *it = self.timeout_node;
  dht::Node *const head = it;
  while (it) {
    if (it == needle) {
      return it;
    }
    it = it->timeout_next;

    if (it == head) {
      break;
    }
  }
  return nullptr;
}

//=====================================
template <typename T>
static bool
debug_is_cycle(T *const head) noexcept {
  T *it = head;

  if (it) {
  Lit:
    if (it) {
      T *const next = it->timeout_next;
      assertx(next);
      assertx(it == next->timeout_priv);

      it = next;
      if (it != head) {
        goto Lit;
      }
    } else {
      assertx(head);
    }
  }

  return true;
}

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

//=====================================
template <typename T>
void
internal_unlink(T *&head, T *const node) noexcept {
  assertx(node);

  T *priv = node->timeout_priv;
  T *next = node->timeout_next;
  assertxs(priv, priv, next);
  assertxs(next, next, priv);

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
unlink(dht::DHT &self, dht::Node *contact) noexcept {
  assertx(debug_is_cycle(self.timeout_node));

  unlink(self.timeout_node, contact);

  assertx(!contact->timeout_next);
  assertx(!contact->timeout_priv);

  assertx(debug_is_cycle(self.timeout_node));
} // timeout::unlink()

void
unlink(dht::DHT &self, dht::Peer *peer) noexcept {
  return internal_unlink(self.timeout_peer, peer);
} // timeout::unlink()

//=====================================
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
append_all(dht::DHT &self, dht::Node *node) noexcept {
  assertx(debug_is_cycle(self.timeout_node));

  internal_append_all(self.timeout_node, node);

  assertx(debug_is_cycle(self.timeout_node));
} // timeout::append_all()

void
append_all(dht::DHT &self, dht::Peer *peer) noexcept {
  assertx(debug_is_cycle(self.timeout_peer));

  internal_append_all(self.timeout_peer, peer);

  assertx(debug_is_cycle(self.timeout_peer));
} // timeout::append_all()

//=====================================
template <typename T>
static void
internal_insert(T *priv, T *subject, T *next) noexcept {
  if (priv) {
    assertx(priv);
    assertx(next);

    priv->timeout_next = subject;
    next->timeout_priv = subject;

    subject->timeout_priv = priv;
    subject->timeout_next = next;

  } else {
    assertx(!priv);
    assertx(!next);

    subject->timeout_priv = subject->timeout_next = subject;
  }
}

void
prepend(dht::DHT &self, dht::Node *subject) noexcept {
  assertx(debug_is_cycle(self.timeout_node));

  assertx(subject);
  assertx(subject->timeout_next == nullptr);
  assertx(subject->timeout_priv == nullptr);

  dht::Node *const head = self.timeout_node;
  internal_insert(head ? head->timeout_priv : nullptr, subject, head);
  self.timeout_node = subject;

  assertx(debug_is_cycle(self.timeout_node));
}

//=====================================
void
insert_new(
    dht::DHT &dht,
    dht::Node *ret) noexcept { // TODO why is this inserted into the front?
  return prepend(dht, ret);
}

//=====================================
void
insert(dht::Peer *priv, dht::Peer *subject, dht::Peer *next) noexcept {
  internal_insert(priv, subject, next);
}

//=====================================
template <typename T>
static T *
internal_take(Timestamp now, sp::Milliseconds timeout, T *&head) noexcept {
  auto is_expired = [now, timeout](auto &node) { //
    return (node.req_sent + timeout) < now;
  };

  T *result = nullptr;

  if (head) {
    T *current = head;
    if (is_expired(*current)) {
      unlink(head, current);
      result = current;
      assertx(!result->timeout_next);
      assertx(!result->timeout_priv);
    }
  }

  return result;
} // timeout::take()

dht::Node *
take_node(dht::DHT &self, sp::Milliseconds timeout) noexcept {
  assertx(debug_is_cycle(self.timeout_node));

  Timestamp now = self.now;
  dht::Node *const result = internal_take(now, timeout, self.timeout_node);

  assertx(debug_is_cycle(self.timeout_node));
  return result;
}

dht::Peer *
take_peer(dht::DHT &self, sp::Milliseconds timeout) noexcept {
  dht::Peer *head = self.timeout_peer;

  Timestamp now = self.now;
  auto is_expired = [now, timeout](auto &node) { //
    return (node.activity + timeout) > now;
  };

  assertx(debug_is_cycle(self.timeout_peer));

  if (head) {
    if (is_expired(*head)) {
      unlink(self, head);
      assertx(!head->timeout_next);
      assertx(!head->timeout_priv);

      assertx(debug_is_cycle(self.timeout_peer));
      return head;
    }
  }

  return nullptr;
}

//=====================================
} // namespace timeout
