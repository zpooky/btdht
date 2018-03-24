#include "timeout.h"
#include "gtest/gtest.h"

static std::size_t
is_cycle(dht::DHT &dht) noexcept {
  std::size_t result = 0;
  dht::Node *const head = dht.timeout_node;
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
  dht::DHT dht(sock, c, r);
  ASSERT_EQ(std::size_t(0), is_cycle(dht));

  dht::Node n1;
  dht::Node n2;
  dht::Node n3;
  {
    timeout::prepend(dht, &n1);
    ASSERT_EQ(std::size_t(1), is_cycle(dht));

    timeout::prepend(dht, &n2);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::prepend(dht, &n3);
    ASSERT_EQ(std::size_t(3), is_cycle(dht));
  }

  {
    timeout::unlink(dht, &n2);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::append_all(dht, &n2);
    ASSERT_EQ(std::size_t(3), is_cycle(dht));
  }

  {
    timeout::unlink(dht, &n2);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::unlink(dht, &n1);
    ASSERT_EQ(std::size_t(1), is_cycle(dht));

    timeout::unlink(dht, &n3);
    ASSERT_EQ(std::size_t(0), is_cycle(dht));

    timeout::append_all(dht, &n2);
    ASSERT_EQ(std::size_t(1), is_cycle(dht));

    timeout::append_all(dht, &n1);
    ASSERT_EQ(std::size_t(2), is_cycle(dht));

    timeout::append_all(dht, &n3);
    ASSERT_EQ(std::size_t(3), is_cycle(dht));
  }
}
