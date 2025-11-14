#include "hot_loop_ir.h"
#include <rufus.hpp>

#include <array>
#include <iostream>
#include <vector>

// Test helper
void test_jit(RuFuS &RS, const std::string &func_str, int N) {
    using FuncType = void (*)(float *);
    auto hot_loop_jit = RS.compile<FuncType>(func_str, {{"N", N}});

    constexpr int N_MAX = 1024;
    alignas(64) std::array<float, N_MAX> testarr;
    testarr.fill(1.0f);

    hot_loop_jit(testarr.data());

    if (testarr[0] != 2.0f || testarr[N - 1] != 2.0f)
        std::cerr << "Test failed for N=" << N << "\n";
    else
        std::cout << "Test passed for N=" << N << "\n";
}

void std_vector_example(RuFuS &RS, int N) {
    auto test_func =
        RS.compile<void (*)(std::vector<float> &)>("hot_loop(std::vector<float,std::allocator<float>>&)", {{"N", N}});

    std::vector<float> vec(N, 1.0f);
    test_func(vec);
    if (vec[0] != 2.0f || vec[N - 1] != 2.0f)
        std::cerr << "Test (std::vector) failed for N=" << N << "\n";
    else
        std::cout << "Test (std::vector) passed for N=" << N << "\n";
}

int main(int argc, char **argv) {
    RuFuS RS;

    // Batch specialization
    RS.load_ir_string(rufus::embedded::hot_loop_ir)
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

    // C++ types are a bit more annoying due to the need to fully specify the types
    // ...so we do them separately
    std_vector_example(RS, 64);

    // Prints out things like available functions and their signatures
    RS.print_debug_info();

    // Optional: print final module IR
    RS.print_module_ir();

    return 0;
}
