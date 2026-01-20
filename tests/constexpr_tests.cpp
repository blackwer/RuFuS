// External
#include <gmock/gmock.h>  // for EXPECT_THAT, HasSubstr, etc
#include <gtest/gtest.h>  // for TEST, ASSERT_NO_THROW, etc

// C++ core
#include <array>
#include <iostream>
#include <rufus.hpp>
#include <vector>

// Auto-generated IR
#include "constexpr_test_utils_ir.h"

TEST(RufusConstexprUnitTests, IfConstexprSwitch) {
  RuFuS RS;
  RS.load_ir_string(rufus::embedded::constexpr_test_utils_ir)
    .specialize_function("is_even_or_odd(int)", {{"flag", true}})
    .optimize();

  using FuncType = bool (*)(int);
  auto is_even = RS.compile<FuncType>("is_even_or_odd(int)", {{"check_even", true}});

  EXPECT_TRUE(is_even(4));
  EXPECT_FALSE(is_even(5));
}
