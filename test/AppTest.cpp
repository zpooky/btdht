#include "gtest/gtest.h"

#include <app.h>

TEST(MainTest, dummy) {
  ASSERT_EQ(1, sp::fun(1));
  ASSERT_EQ(1, sp::inl());
}
