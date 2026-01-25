// External
#include <gmock/gmock.h> // for EXPECT_THAT, HasSubstr, etc
#include <gtest/gtest.h> // for TEST, ASSERT_NO_THROW, etc

// C++ core
#include <array>
#include <iostream>
#include <rufus.hpp>
#include <vector>

// Auto-generated IR
#include "constexpr_test_utils_ir.h"

void test_jit(bool check_even) {
    std::string func_str = "is_even_or_odd(int, bool)";
    using FuncType = bool (*)(int);

    RuFuS RS;
    RS.load_ir_string(rufus::embedded::constexpr_test_utils_ir)
        .specialize_function(func_str, {{"check_even", check_even}})
        .optimize();
    auto is_even_or_odd = RS.compile<FuncType>(func_str, {{"check_even", check_even}});
    if (check_even) {
        EXPECT_TRUE(is_even_or_odd(4));
        EXPECT_FALSE(is_even_or_odd(5));
    } else {
        EXPECT_FALSE(is_even_or_odd(4));
        EXPECT_TRUE(is_even_or_odd(5));
    }
}

void test_template_jit(bool check_even) {
    std::string func_str =
        check_even ? "bool is_even_or_odd_template<true>(int)" : "bool is_even_or_odd_template<false>(int)";
    using FuncType = bool (*)(int);

    RuFuS RS;
    RS.load_ir_string(rufus::embedded::constexpr_test_utils_ir)
        .specialize_function(func_str, {{"check_even", check_even}})
        .optimize();
    auto is_even_or_odd = RS.compile<FuncType>(func_str, {{"check_even", check_even}});
    if (check_even) {
        EXPECT_TRUE(is_even_or_odd(4));
        EXPECT_FALSE(is_even_or_odd(5));
    } else {
        EXPECT_FALSE(is_even_or_odd(4));
        EXPECT_TRUE(is_even_or_odd(5));
    }
}

void test_constexpr_jit(bool check_even) {
    std::string func_str =
        check_even ? "bool is_even_or_odd_constexpr<true>(int)" : "bool is_even_or_odd_constexpr<false>(int)";
    using FuncType = bool (*)(int);

    RuFuS RS;
    RS.load_ir_string(rufus::embedded::constexpr_test_utils_ir)
        .specialize_function(func_str, {{"check_even", check_even}})
        .optimize();

    auto is_even_or_odd = RS.compile<FuncType>(func_str, {{"check_even", check_even}});
    if (check_even) {
        EXPECT_TRUE(is_even_or_odd(4));
        EXPECT_FALSE(is_even_or_odd(5));
    } else {
        EXPECT_FALSE(is_even_or_odd(4));
        EXPECT_TRUE(is_even_or_odd(5));
    }
}

TEST(RufusConstexprUnitTests, IfSwitch) {
    test_jit(true);
    test_jit(false);
}

TEST(RufusConstexprUnitTests, IfSwitchTemplate) {
    test_template_jit(true);
    test_template_jit(false);
}

TEST(RufusConstexprUnitTests, IfSwitchConstexpr) {
    test_constexpr_jit(true);
    test_constexpr_jit(false);
}
