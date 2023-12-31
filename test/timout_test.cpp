#include "shared.h"
#include "timeout.h"

#include "gtest/gtest.h"

static std::size_t
is_cycle(dht::DHT &dht) noexcept {
  std::size_t result = 0;
  dht::Node *const head = dht.tb.timeout->timeout_node;
  if (head) {
    dht::Node *it = head;
  Lit:
    if (head) {
      dht::Node *next = it->timeout_next;
      assert(it == next->timeout_priv);

      ++result;
      it = next;
      if (it != head) {
        goto Lit;
      }
    } else {
      assert(head);
    }
  }
  return result;
}

TEST(TimeoutTest, test) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, c, r, now);
  ASSERT_EQ(std::size_t(0), is_cycle(dht));

  dht::Node n1;
  dht::Node n2;
  dht::Node n3;
  {
    timeout::prepend(*dht.tb.timeout, &n1);
    ASSERT_EQ(std::size_t(1), is_cycle(dht));

    timeout::prepend(*dht.tb.timeout, &n2);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::prepend(*dht.tb.timeout, &n3);
    ASSERT_EQ(std::size_t(3), is_cycle(dht));
  }

  {
    timeout::unlink(*dht.tb.timeout, &n2);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::append_all(*dht.tb.timeout, &n2);
    ASSERT_EQ(std::size_t(3), is_cycle(dht));
  }

  {
    timeout::unlink(*dht.tb.timeout, &n2);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::unlink(*dht.tb.timeout, &n1);
    ASSERT_EQ(std::size_t(1), is_cycle(dht));

    timeout::unlink(*dht.tb.timeout, &n3);
    ASSERT_EQ(std::size_t(0), is_cycle(dht));

    timeout::append_all(*dht.tb.timeout, &n2);
    ASSERT_EQ(std::size_t(1), is_cycle(dht));

    timeout::append_all(*dht.tb.timeout, &n1);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::append_all(*dht.tb.timeout, &n3);
    ASSERT_EQ(std::size_t(3), is_cycle(dht));
  }
}

TEST(TimeoutTest, test2) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, c, r, now);
  dht.now = sp::Timestamp(0);
  ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)), nullptr);

  dht::Node node0;
  node0.req_sent = sp::Timestamp(0);
  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  timeout::insert_new(*dht.tb.timeout, &node0);
  ASSERT_TRUE(dht.tb.timeout->timeout_node);
  {
    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(0)),
              nullptr);

    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)), &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);

    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(0)),
              nullptr);
    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)),
              nullptr);
  }
  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  timeout::append_all(*dht.tb.timeout, &node0);
  ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  {
    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(0)),
              nullptr);

    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)), &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);

    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(0)),
              nullptr);
    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)),
              nullptr);
  }
  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  timeout::prepend(*dht.tb.timeout, &node0);
  ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  {
    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(0)),
              nullptr);

    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)), &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);

    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(0)),
              nullptr);
    ASSERT_EQ(timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1)),
              nullptr);
  }
}

TEST(TimeoutTest, test3) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, c, r, now);

  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  dht::Node node0;
  {
    timeout::insert_new(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }
  {
    timeout::unlink(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  }

  {
    timeout::insert_new(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }
  dht::Node node1;
  {
    timeout::insert_new(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }

  {
    timeout::insert_new(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  }
}

TEST(TimeoutTest, test4) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, c, r, now);

  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  dht::Node node0;
  {
    timeout::prepend(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }
  {
    timeout::unlink(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  }

  {
    timeout::prepend(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }
  dht::Node node1;
  {
    timeout::prepend(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }

  {
    timeout::prepend(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  }
}

TEST(TimeoutTest, test5) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, c, r, now);

  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  dht::Node node0;
  {
    timeout::append_all(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }
  {
    timeout::unlink(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  }

  {
    timeout::append_all(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }
  dht::Node node1;
  {
    timeout::append_all(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }

  {
    timeout::append_all(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node0);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node0);
    ASSERT_EQ(dht.tb.timeout->timeout_node, &node1);
  }

  {
    timeout::unlink(*dht.tb.timeout, &node1);
    ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
  }
}

TEST(TimeoutTest, test_arr) {
  fd sock(-1);
  Contact c(0, 0);
  prng::xorshift32 r(1);
  Timestamp now = sp::now();
  dht::DHT dht(sock, sock, c, r, now);

  sp::UinStaticArray<dht::Node, 1024> a;
  while (!is_full(a)) {
    dht::Node in;
    auto c = insert(a, in);
  Lit : {
    ASSERT_FALSE(timeout::debug_find_node(*dht.tb.timeout, c));
    auto type = uniform_dist(dht.random, 0, 3);
    if (type == 0) {
      timeout::insert_new(*dht.tb.timeout, c);
    } else if (type == 1) {
      timeout::append_all(*dht.tb.timeout, c);
    } else if (type == 2) {
      timeout::prepend(*dht.tb.timeout, c);
    } else {
      assertx(false);
    }
  }

    ASSERT_EQ(timeout::debug_find_node(*dht.tb.timeout, c), c);
    ASSERT_EQ(timeout::debug_count_nodes(*dht.tb.timeout), length(a));
    // 25%
    if (uniform_dist(dht.random, 0, 4) == 0) {
      c = timeout::take_node(*dht.tb.timeout, sp::Milliseconds(1));
      ASSERT_EQ(timeout::debug_count_nodes(*dht.tb.timeout), length(a) - 1);
      ASSERT_FALSE(timeout::debug_find_node(*dht.tb.timeout, c));
      ASSERT_TRUE(c);
      goto Lit;
    }

    // 25%
    if (uniform_dist(dht.random, 0, 4) == 0) {
      c = &a[uniform_dist(dht.random, 0, length(a))];
      timeout::unlink(*dht.tb.timeout, c);
      ASSERT_EQ(timeout::debug_count_nodes(*dht.tb.timeout), length(a) - 1);
      ASSERT_FALSE(timeout::debug_find_node(*dht.tb.timeout, c));
      goto Lit;
    }

    ASSERT_EQ(timeout::debug_count_nodes(*dht.tb.timeout), length(a));
  }
  for (std::size_t i = 0; i < length(a); ++i) {
    ASSERT_EQ(timeout::debug_count_nodes(*dht.tb.timeout), length(a) - i);
    timeout::unlink(*dht.tb.timeout, &a[i]);
  }
  ASSERT_EQ(timeout::debug_count_nodes(*dht.tb.timeout), 0);
  ASSERT_EQ(dht.tb.timeout->timeout_node, nullptr);
}
