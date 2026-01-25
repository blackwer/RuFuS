// External
#include <gmock/gmock.h> // for EXPECT_THAT, HasSubstr, etc
#include <gtest/gtest.h> // for TEST, ASSERT_NO_THROW, etc

// C++ core
#include <array>
#include <iostream>
#include <rufus.hpp>
#include <vector>

// Auto-generated IR
#include "hot_loop_test_utils_ir.h"

// Test helper
void test_jit(RuFuS &RS, const std::string &func_str, int N) {
    using FuncType = void (*)(float *);
    auto hot_loop_jit = RS.compile<FuncType>(func_str, {{"N", N}});

    constexpr int N_MAX = 1024;
    alignas(64) std::array<float, N_MAX> testarr;
    testarr.fill(1.0f);

    hot_loop_jit(testarr.data());

    EXPECT_FLOAT_EQ(testarr[0], 2.0f) << "Test failed for N=" << N;
    EXPECT_FLOAT_EQ(testarr[N - 1], 2.0f) << "Test failed for N=" << N;
}

void std_vector_example(RuFuS &RS, int N) {
    auto test_func =
        RS.load_ir_string(rufus::embedded::hot_loop_test_utils_ir)
            .compile<void (*)(std::vector<float> &)>("hot_loop(std::vector<float,std::allocator<float>>&)", {{"N", N}});

    std::vector<float> vec(N, 1.0f);
    test_func(vec);
    EXPECT_FLOAT_EQ(vec[0], 2.0f) << "Test (std::vector) failed for N=" << N;
    EXPECT_FLOAT_EQ(vec[N - 1], 2.0f) << "Test (std::vector) failed for N=" << N;
}

TEST(ExampleTest, Test) { EXPECT_TRUE(true) << "Basic test framework check failed"; }

TEST(RufusUnitTests, HotLoopSpecialization) {
    RuFuS RS;

    // Batch specialization
    RS.load_ir_string(rufus::embedded::hot_loop_test_utils_ir)
        .specialize_function("hot_loop(float*,int)", {{"N", 64}})
        .specialize_function("hot_loop_const(float*)", {{"N", 64}})
        .specialize_function("hot_loop_inlining(float*,int)", {{"N", 64}})
        .optimize();

    for (auto &N : {64, 65}) {
        test_jit(RS, "hot_loop(float*,int)", N);
        test_jit(RS, "hot_loop_const(float*)", N);
        test_jit(RS, "hot_loop_inlining(float*,int)", N);

        // Template functions require return type
        test_jit(RS, "void hot_loop_template<float>(float*,int)", N);
    }
}

TEST(RufusUnitTests, EmptyOptimize) {
    RuFuS RS;
    RS.load_ir_string(rufus::embedded::hot_loop_test_utils_ir).optimize();
}

TEST(RufusUnitTests, MultipleRuFuSInstances) {
    RuFuS RS1;
    RuFuS RS2;

    // Batch specialization
    RS1.load_ir_string(rufus::embedded::hot_loop_test_utils_ir)
        .specialize_function("hot_loop(float*,int)", {{"N", 64}})
        .specialize_function("hot_loop_const(float*)", {{"N", 64}})
        .optimize();
    RS2.load_ir_string(rufus::embedded::hot_loop_test_utils_ir)
        .specialize_function("hot_loop_inlining(float*,int)", {{"N", 64}})
        .optimize();

    for (auto &N : {64, 65}) {
        test_jit(RS1, "hot_loop(float*,int)", N);
        test_jit(RS1, "hot_loop_const(float*)", N);
        test_jit(RS2, "hot_loop_inlining(float*,int)", N);

        // Template functions require return type
        test_jit(RS1, "void hot_loop_template<float>(float*,int)", N);
    }
}

TEST(RufusUnitTests, EvalAllPairs) {
    RuFuS RS;
    RS.load_ir_string(rufus::embedded::hot_loop_test_utils_ir)
        .specialize_function("evaluate_all_pairs_inv_r2_struct(float*,float*,float*,int,int)",
                             {{"Nsrc", 64}, {"Ntrg", 64}})
        .optimize();
    RS.load_ir_string(rufus::embedded::hot_loop_test_utils_ir)
        .specialize_function("evaluate_all_pairs_inv_r2_lambda(float*,float*,float*,int,int)",
                             {{"Nsrc", 64}, {"Ntrg", 64}})
        .optimize();

    std::vector<float> coeffs{
        1.340418974956820e-03,  -6.599369969180820e-03, 1.490307518448090e-02, -2.093949273676980e-02,
        2.107881727833481e-02,  -1.675447756809429e-02, 1.153573427436465e-02, -7.167326866171437e-03,
        3.494340256858195e-03,  -1.811569682012156e-03, 2.526431600085065e-03, -1.709903001756345e-03,
        -7.760281837689070e-04, 6.225228333113239e-04,  7.224764067524717e-04, -4.656557370053271e-04};
    RS.specialize_function("evaluate_all_pairs_laplace_polynomial(float*,float*,float*,int,int,float*,int)",
                           {{"Nsrc", 64}, {"Ntrg", 64}, {"n_coefs", coeffs.size()}})
        .optimize();
}

TEST(RufusUnitTests, StdVector) {
    // C++ types are a bit more annoying due to the need to fully specify the types
    // ...so we do them separately
    RuFuS RS;
    std_vector_example(RS, 64);
}
