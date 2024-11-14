#ifndef SP_MAINLINE_DHT_TIMEOUT_IMPL_H
#define SP_MAINLINE_DHT_TIMEOUT_IMPL_H
#include "dht.h"
#include "shared.h"
#include "Log.h"

namespace timeout {
template <typename F>
static inline bool
for_all_node(dht::DHTMetaRoutingTable &routing_table, sp::Milliseconds timeout,
             F f, dht::DHT *self = nullptr) {
  const dht::Node *start = nullptr;
  assertx(debug_assert_all(routing_table));
  assertx(routing_table.tb.timeout);
  bool result = true;
Lstart: {
  dht::Node *const node =
      timeout::take_node(*routing_table.tb.timeout, timeout);
  logger::routing::head_node(routing_table, timeout);
  if (node) {
    if (node == start) {
      assertx(!timeout::debug_find_node(*routing_table.tb.timeout, node));
      timeout::prepend(*routing_table.tb.timeout, node);
      assertx(node->timeout_next);
      assertx(node->timeout_priv);
      assertx(timeout::debug_find_node(*routing_table.tb.timeout, node) ==
              node);
      assertx(debug_assert_all(routing_table));
      return true;
    }

    if (!start) {
      start = node;
    }
    assertx(!timeout::debug_find_node(*routing_table.tb.timeout, node));

    // printf("node: %s\n", to_hex(node->id));

    assertx(!node->timeout_next);
    assertx(!node->timeout_priv);

    if (node->good) {
      if (self) {
        if (dht::should_mark_bad(*self, *node)) { // TODO ??
          node->good = false;
          routing_table.bad_nodes++;
        }
      }
    }

    if (f(routing_table, *node)) {
      timeout::append_all(*routing_table.tb.timeout, node);
      assertx(node->timeout_next);
      assertx(node->timeout_priv);
      assertx(timeout::debug_find_node(*routing_table.tb.timeout, node) ==
              node);
      assertx(debug_assert_all(routing_table));

      goto Lstart;
    } else {
      timeout::prepend(*routing_table.tb.timeout, node);
      assertx(node->timeout_next);
      assertx(node->timeout_priv);
      assertx(timeout::debug_find_node(*routing_table.tb.timeout, node) ==
              node);
      assertx(debug_assert_all(routing_table));

      result = false;
    }
  }
}

  return result;
}
} // namespace timeout

#endif
