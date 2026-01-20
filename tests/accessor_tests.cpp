// External
#include <gmock/gmock.h>  // for EXPECT_THAT, HasSubstr, etc
#include <gtest/gtest.h>  // for TEST, ASSERT_NO_THROW, etc

// C++ core
#include <cstddef>
#include <array>
#include <iostream>
#include <rufus.hpp>
#include <vector>
#include <chrono>
#include <variant>

// Test helper
#include "accessor.hpp"

// Auto-generated IR
#include "accessor_test_utils_ir.h"

// Need improved function searching for T const& vs const T& as well as std::size_t vs unsigned long equivalence
// aliases don't work either nor do default templates. std::variant is a nightmare

void test_jit(double x_shared_value, std::vector<double>& x_vec, 
              std::vector<double>& y_vec,
              bool use_shared) {
  std::string func_str = "axpby(double, Accessor const&, double, Accessor const&, unsigned long)";
  using FuncType = void (*)(double, const Accessor&, double, Accessor&);

  RuFuS RS;
  RS.load_ir_string(rufus::embedded::accessor_test_utils_ir)
    .specialize_function(func_str, {{"N", y_vec.size()}})
    .optimize();
  auto axpby_jit = RS.compile<FuncType>(func_str, {{"N", y_vec.size()}});

  double a = 2.0;
  double b = 3.0;
  if (use_shared) {
    Accessor ax(x_shared_value);
    Accessor ay(y_vec);
    axpby_jit(a, ax, b, ay);
  } else {
    Accessor ax(x_vec);
    Accessor ay(y_vec);
    axpby_jit(a, ax, b, ay);
  }
}

TEST(RufusAccessorTests, AccessorSharedValue) {
  size_t N = rand() % 1000 + 1000;  // between 1000 and 1999 to avoid constexpr optimizations
  double x_shared_value = 1.0;
  std::vector<double> x_vec(N, 1.0);
  std::vector<double> y_vec(N, 1.0);

  // time it shared
  auto start = std::chrono::high_resolution_clock::now();
  test_jit(x_shared_value, x_vec, y_vec, true);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  std::cout << "AccessorSharedValue took " << duration.count() << " ms\n";

  for (size_t i = 0; i < N; ++i) {
    EXPECT_DOUBLE_EQ(y_vec[i], 2.0 * x_shared_value + 3.0 * 1.0);
  }

  // time it non-shared
  auto start2 = std::chrono::high_resolution_clock::now();
  test_jit(x_shared_value, x_vec, y_vec, false);
  auto end2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration2 = end2 - start2;
  std::cout << "AccessorNonSharedValue took " << duration2.count() << " ms\n";

  for (size_t i = 0; i < N; ++i) {
    EXPECT_DOUBLE_EQ(y_vec[i], 2.0 * 1.0 + 3.0 * (2.0 * x_shared_value + 3.0 * 1.0));
  }

  // time it raw 
  auto start3 = std::chrono::high_resolution_clock::now();
  double a = 2.0;
  double b = 3.0;
  Accessor ax(x_vec);
  Accessor ay(y_vec);
  for (size_t i = 0; i < N; ++i) {
    y_vec[i] = a * ax(i) + b * ay(i);
  }
  auto end3 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration3 = end3 - start3;
  std::cout << "NoAccessorRawValue took " << duration3.count() << " ms" << std::endl;

  for (size_t i = 0; i < N; ++i) {
    EXPECT_DOUBLE_EQ(y_vec[i], 2.0 * 1.0 + 3.0 * (2.0 * 1.0 + 3.0 * (2.0 * x_shared_value + 3.0 * 1.0)));
  }
}

using vector_or_scalar = std::variant<double, std::vector<double>>;

void test_variant_jit(const vector_or_scalar& x, vector_or_scalar &y, size_t N) {           
  std::string func_str = "axpby(double,std::variant<double,std::vector<double,std::allocator<double>>>const&,double,std::variant<double,std::vector<double,std::allocator<double>>>&,unsignedlong,bool,bool)";
  using FuncType = void (*)(double, const vector_or_scalar&, double, vector_or_scalar&, std::size_t, bool, bool);

  bool is_x_shared = std::holds_alternative<double>(x);
  bool is_y_shared = std::holds_alternative<double>(y);

  RuFuS RS;
  RS.load_ir_string(rufus::embedded::accessor_test_utils_ir)
    .specialize_function(func_str, {{"N", N}, {"is_x_shared", is_x_shared}, {"is_y_shared", is_y_shared}})
    .optimize();
  auto axpby_jit = RS.compile<FuncType>(func_str, {{"N", N}, {"is_x_shared", is_x_shared}, {"is_y_shared", is_y_shared}});

  double a = 2.0;
  double b = 3.0;
  axpby_jit(a, x, b, y, N, is_x_shared, is_y_shared);
}


template<bool is_x_shared, bool is_y_shared>
void axpby_constexpr(double a, const vector_or_scalar &x, double b, vector_or_scalar &y, std::size_t N) {
  for (std::size_t i = 0; i < N; ++i) {
    double x_val = is_x_shared ? std::get<double>(x) : std::get<std::vector<double>>(x)[i];
    double y_val = is_y_shared ? std::get<double>(y) : std::get<std::vector<double>>(y)[i];
    double result = a * x_val + b * y_val;
    if (is_y_shared) {
      std::get<double>(y) = result;
    } else {
      std::get<std::vector<double>>(y)[i] = result;
    }
  }
}


TEST(RufusAccessorTests, VariantAccessor) {
  size_t N = rand() % 1000 + 1000;  // between 1000 and 1999 to avoid constexpr optimizations
  double x_shared_value = 1.0;
  std::vector<double> x_vec(N, 1.0);
  std::vector<double> y_vec(N, 1.0);
  vector_or_scalar x_shared = x_shared_value;
  vector_or_scalar x_non_shared = x_vec;
  vector_or_scalar y_variant = y_vec;

  // time it shared
  auto start = std::chrono::high_resolution_clock::now();
  test_variant_jit(x_shared, y_variant, N);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  std::cout << "VariantAccessorSharedValue took " << duration.count() << " ms\n";

  for (size_t i = 0; i < N; ++i) { 
    EXPECT_DOUBLE_EQ(std::get<std::vector<double>>(y_variant)[i], 2.0 * x_shared_value + 3.0 * 1.0);
  }

  // time it non-shared
  auto start2 = std::chrono::high_resolution_clock::now();
  test_variant_jit(x_non_shared, y_variant, N);
  auto end2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration2 = end2 - start2;
  std::cout << "VariantAccessorNonSharedValue took " << duration2.count() << " ms\n";

  for (size_t i = 0; i < N; ++i) {
    EXPECT_DOUBLE_EQ(std::get<std::vector<double>>(y_variant)[i], 2.0 * 1.0 + 3.0 * (2.0 * x_shared_value + 3.0 * 1.0));
  }

  // time it constexpr 
  auto start3 = std::chrono::high_resolution_clock::now();
  axpby_constexpr<false, false>(2.0, x_non_shared, 3.0, y_variant, N);
  auto end3 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration3 = end3 - start3;
  std::cout << "NoAccessorRawValue took " << duration3.count() << " ms" << std::endl;

  for (size_t i = 0; i < N; ++i) {
    EXPECT_DOUBLE_EQ(std::get<std::vector<double>>(y_variant)[i], 2.0 * 1.0 + 3.0 * (2.0 * 1.0 + 3.0 * (2.0 * x_shared_value + 3.0 * 1.0)));
  }
}

