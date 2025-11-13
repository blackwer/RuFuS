#include "hot_loop_ir.h"
#include <rufus.hpp>

#include <array>
#include <iostream>
#include <vector>

// Test helper
template <int N>
void test_jit(RuFuS &RS) {
    using FuncType = void (*)(float *);
    auto hot_loop_jit = RS.compile<FuncType>("hot_loop(float*,int)", {{"N", N}});

    alignas(64) std::array<float, N> testarr;
    testarr.fill(1.0f);

    hot_loop_jit(testarr.data());

    if (testarr[0] != 2.0f || testarr[N - 1] != 2.0f)
        std::cerr << "Test failed for N=" << N << "\n";
    else
        std::cout << "Test passed for N=" << N << "\n";
}

int main(int argc, char **argv) {
    RuFuS RS;

    // Batch specialization (more efficient)
    RS.load_ir_string(rufus::embedded::hot_loop_ir)
        .specialize_function("hot_loop(float*,int)", {{"N", 64}})
        .specialize_function("hot_loop(float*,int)", {{"N", 65}})
        .specialize_function("hot_loop_const(float*)", {{"N", 64}})
        .optimize();

    test_jit<64>(RS);
    test_jit<65>(RS);

    RS.specialize_function("hot_loop(float*,int)", {{"N", 66}}).optimize();
    test_jit<66>(RS);

    // Template functions require return type
    RS.specialize_function("void hot_loop_template<float>(float*,int)", {{"N", 65}}).optimize();
    RS.specialize_function("hot_loop(std::vector<float,std::allocator<float>>&)", {{"N", 64}}).optimize();
    auto test_func =
        RS.compile<void (*)(std::vector<float> &)>("hot_loop(std::vector<float,std::allocator<float>>&)", {{"N", 64}});

    std::vector<float> vec(64, 1.0f);
    test_func(vec);
    if (vec[0] != 2.0f || vec[64 - 1] != 2.0f)
        std::cerr << "Test (std::vector) failed for N=" << 64 << "\n";
    else
        std::cout << "Test (std::vector) passed for N=" << 64 << "\n";

    // Prints out things like available functions and their signatures
    RS.print_debug_info();

    // Optional: print final module IR
    // RS.print_module_ir();

    return 0;
}
