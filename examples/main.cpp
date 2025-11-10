#include <rufus.hpp>

#include <array>
#include <iostream>

// Test helper
template <int N>
void test_jit(RuFuS &RS) {
    using FuncType = void (*)(float *);
    FuncType hot_loop_jit = reinterpret_cast<FuncType>(RS.compile("hot_loop(float*,int)", {{"N", N}}));

    alignas(64) std::array<float, N> testarr;
    testarr.fill(1.0f);

    hot_loop_jit(testarr.data());

    if (testarr[0] != 2.0f || testarr[N - 1] != 2.0f)
        std::cerr << "Test failed for N=" << N << "\n";
    else
        std::cout << "Test passed for N=" << N << "\n";
}

int main(int argc, char **argv) {
    std::string ir_file = argc > 1 ? argv[1] : "hot_loop.ll";

    RuFuS RS;

    // Batch specialization (more efficient)
    RS.load_ir_file(ir_file)
        .specialize_function("hot_loop(float*,int)", {{"N", 64}})
        .specialize_function("hot_loop(float*,int)", {{"N", 65}})
        .specialize_function("hot_loop_const(float*)", {{"N", 64}})
        .optimize();

    test_jit<64>(RS);
    test_jit<65>(RS);

    // On-demand specialization. It's suboptimal since it re-optimizes the entire module each time.
    test_jit<66>(RS);

    // Prints out things like available functions and their signatures
    RS.print_debug_info();

    // Optional: print final module IR
    RS.print_module_ir();

    return 0;
}
