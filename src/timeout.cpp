#include "timeout.h"
#include <util/assert.h>

namespace timeout {

//=====================================
Timeout::Timeout(Timestamp &n) noexcept
    : timeout_next(0)
    , timeout_node(nullptr)
    , now(n) {
}

//=====================================
TimeoutBox::TimeoutBox(Timestamp &now) noexcept
    : dummy(now)
    , timeout() {
  timeout = &dummy;
}

//=====================================
std::size_t
debug_count_nodes(const Timeout &self) noexcept {
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
debug_find_node(const Timeout &self, const dht::Node *needle) noexcept {
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
debug_is_cycle(T *const head, std::size_t &length) noexcept {
  length = head ? 1 : 0;
#if 0
  T *it = head;

  if (it) {
  Lit:
    if (it) {
      T *const next = it->timeout_next;
      assertx(next);
      assertx(it == next->timeout_priv);

      it = next;
      if (it != head) {
        ++length;
        goto Lit;
      }
    } else {
      assertx(head);
    }
  }
#else
  if (head) {
    assertx(head->timeout_next->timeout_priv == head);
    assertx(head->timeout_priv->timeout_next == head);
    // printf("%p, ", (void *)head);
    for (T *it = head->timeout_next; it != head; it = it->timeout_next) {
      // printf("%p, ", (void *)it);
      assertx(it);
      T *const next = it->timeout_next;
      assertx(next);
      assertx(it == next->timeout_priv);

      ++length;
    }
    // printf("\n");
  }
#endif

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
unlink(Timeout &self, dht::Node *contact) noexcept {
  std::size_t length = 0;
  std::size_t after_length = 0;
  assertx(debug_is_cycle(self.timeout_node, length));
  assertx(length > 0);

  unlink(self.timeout_node, contact);

  assertx(!contact->timeout_next);
  assertx(!contact->timeout_priv);

  assertx(debug_is_cycle(self.timeout_node, after_length));
  assertx((length - 1) == after_length);
} // timeout::unlink()

void
unlink(dht::KeyValue &self, dht::Peer *peer) noexcept {
  return internal_unlink(self.timeout_peer, peer);
} // timeout::unlink()

//=====================================
template <typename T>
void
internal_unlink(T *&head, T *const from, T *const to) noexcept {
  assertx(from);

  T *priv = from->timeout_priv;
  T *next = from->timeout_next;
  assertxs(priv, priv, next);
  assertxs(next, next, priv);

  if (priv == from || next == from) {
    assertx(priv == from);
    assertx(next == from);
    priv = to;
    next = to;
  }

  if (priv) {
    priv->timeout_next = to;
  }

  if (next) {
    next->timeout_priv = to;
  }

  if (head == from) {
    head = to;
  }

  to->timeout_priv = priv;
  to->timeout_next = next;

  from->timeout_next = nullptr;
  from->timeout_priv = nullptr;
}

void
move(Timeout &self, dht::Node *from, dht::Node *to) noexcept {
  std::size_t length = 0;
  std::size_t after_length = 0;
  assertx(debug_is_cycle(self.timeout_node, length));

  internal_unlink(self.timeout_node, from, to);

  assertx(!from->timeout_next);
  assertx(!from->timeout_priv);

  assertx(debug_is_cycle(self.timeout_node, after_length));
  assertx(length == after_length);
}

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
append_all(Timeout &self, dht::Node *node) noexcept {
  std::size_t length = 0;
  std::size_t after_length = 0;
  assertx(debug_is_cycle(self.timeout_node, length));

  internal_append_all(self.timeout_node, node);

  assertx(debug_is_cycle(self.timeout_node, after_length));
} // timeout::append_all()

void
append_all(dht::KeyValue &self, dht::Peer *peer) noexcept {
  std::size_t length = 0;
  std::size_t after_length = 0;
  assertx(debug_is_cycle(self.timeout_peer, length));

  internal_append_all(self.timeout_peer, peer);

  assertx(debug_is_cycle(self.timeout_peer, after_length));
} // timeout::append_all()

//=====================================
void
prepend(Timeout &self, dht::Node *subject) noexcept {
  std::size_t length = 0;
  std::size_t after_length = 0;
  assertx(debug_is_cycle(self.timeout_node, length));

  assertx(subject);
  assertx(subject->timeout_next == nullptr);
  assertx(subject->timeout_priv == nullptr);
  // printf("insert(%p)\n", (void *)subject);

  dht::Node *const root = self.timeout_node;
  if (root) {
    dht::Node *const priv = root->timeout_priv;
    dht::Node *const next = root->timeout_next;
    assertx(priv->timeout_next == root);
    assertx(next->timeout_priv == root);

    priv->timeout_next = subject;
    root->timeout_priv = subject;

    subject->timeout_priv = priv;
    subject->timeout_next = root;
  } else {
    subject->timeout_priv = subject->timeout_next = subject;
  }
  self.timeout_node = subject;

  assertx(debug_is_cycle(self.timeout_node, after_length));
  assertxs(length + 1 == after_length, length, after_length);
}

//=====================================
void
insert_new(Timeout &dht, dht::Node *ret) noexcept {
  // TODO why is this inserted into the front?
  return prepend(dht, ret);
}

//=====================================
template <typename T>
static T *
internal_take(const Timestamp &now, sp::Milliseconds timeout,
              T *&head) noexcept {
  auto is_expired = [&now, timeout](auto &node) { //
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
take_node(Timeout &self, sp::Milliseconds timeout) noexcept {
  std::size_t length = 0;
  std::size_t after_length = 0;
  assertx(debug_is_cycle(self.timeout_node, length));

  Timestamp now{self.now};
  dht::Node *const result = internal_take(now, timeout, self.timeout_node);

  assertx(debug_is_cycle(self.timeout_node, after_length));

  return result;
}

dht::Peer *
take_peer(db::DHTMetaDatabase &dht, dht::KeyValue &self,
          sp::Milliseconds timeout) noexcept {

  std::size_t length = 0;
  std::size_t after_length = 0;
  dht::Peer *head = self.timeout_peer;

  auto is_expired = [&](auto &node) {
    return dht.now >= (node.activity + timeout);
  };

  assertx(debug_is_cycle(self.timeout_peer, length));

  if (head) {
    if (is_expired(*head)) {
      unlink(self, head);
      assertx(!head->timeout_next);
      assertx(!head->timeout_priv);

      assertx(debug_is_cycle(self.timeout_peer, after_length));
      assertx(length - 1 == after_length);
      return head;
    }
  } else {
    assertx(after_length == 0);
  }
  assertx(debug_is_cycle(self.timeout_peer, after_length));

  assertxs(after_length == length, after_length, length);
  return nullptr;
}

//=====================================
} // namespace timeout
