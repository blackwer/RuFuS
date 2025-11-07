# RuFuS: Runtime Function Specialization

A proof of concept for doing a very limited JIT in C++. Yes, I know Rufus is already a project, but I came up with this
name in about 20 seconds. Also they don't have the cute capital letters that make it look all snaky.


## Features

+ *Runtime specialization*: Convert runtime function arguments to compile-time constants
+ *Simple API*: Aspirational.


## Requirements

+ LLVM 19 (probably others, but they change their API a lot and I didn't bother testing)
+ cmake 3.16
+ clang++ in your path (to generate the initial IR)
+ A c++17 compatible compiler. You might as well use clang++, but you should be able to build this with any other c++17
  compiler in theory


## Example

I know you've been dying to optimize `arr *= 2` at runtime. This is dumb, but I have actually had to deal with this kind
of crap on a regular basis. Bottlenecked on a very cheap operation that I don't know the size of at runtime. Like I
said: polynomial evaluation.

`src/hot_loop.cpp`
```
void hot_loop(float *arr, int N) {
    arr = (float *)__builtin_assume_aligned(arr, 64);
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2.0f;
    }
}
```

Emit the most basic IR representation of my function(s) of interest.
```
clang++ -S -O0 -fno-discard-value-names -emit-llvm hot_loop.cpp
```

`src/main.cpp`
```
    // desired function type after eating our arguments
    using FuncType = void (*)(float *);
    
    RuntimeSpecializer RS;
    auto hot_loop_N_64 = reinterpre_cast<FuncType>(
        RS.load_ir_file("hot_loop.ll")
          .specialize_function("hot_loop(float*,int)", {{"N", 64}})
          .optimize()
          .compile("hot_loop(float*,int)", {{"N", 64}})
    );
        
    alignas(64) std::array<float, 64> testarr;
    testarr.fill(1.0f);
    hot_loop_jit(testarr.data());

    for (auto el : testarr)
        assert(el == 2.0f);
        
    // for the curious who want to see what the IR looks like
    RS.print_debug_info();
```


## Motivation

Why, you might ask, would you bother with this? This is why we have templates, you might say. Runtime dispatch is a
solved problem, you might say.

And I would respond: you are correct, but dealing with that is AWFUL. I have been programming in C++ for about a
decade. I have been coding C++ professionally in an HPC context for about six of those years. In all that time, I have
reached, what I consider, the status of being an "OK" C++ programmer. This is because C++, while being extremely
powerful, is constantly working against you. The syntax is obtuse. The error messages are terrible. Inspecting types in
a debugger is awful. It is the right tool for the job, but not by virtue of being good, but by the virtue of being the
only viable option for my use cases. I'd like to be a julia programmer, or a rust one, but either limits my audience
and/or my performance.

Here is a rant about why I think this might be a useful idea:

### A Rant

Templates are agitating. It's meta-code. When I'm developing, I want to get the task done. I do not want to labor on
what variables I need to template, how to dispatch them, how to deal with multiple template parameter dispatch,
etc. Single dispatch is annoying enough.

Basic workflow: Write two functions. A templated one that does that work, and a dispatcher that calls the templated
version via a switch or fold expression or std::variant or std::map or std::array or any other crap. I have to fill that
crap out every time I need to runtime dispatch a template parameter. It's easy enough. No problem.

Now I realize I have another parameter that improves performance considerably when a runtime const. Now the workflow
basically looks like

1. Repeat process from before, wrapping the last dispatcher around a new one
2. Fight with an AI or cry for 20 minutes trying to get the syntax right
3. Copy and paste some old code and hack at it while poring through template errors
4. ???
5. success

Fuck. I now I have have another parameter. Or maybe I want to ship a multi-arch binary. Time to dispatch again. I now
have a combinatoric explosion of parameters. Maybe I should change my dispatch strategy? But... I put in so much
work. Fuck. I just want something that works. Please kill me my compile times are several minutes, my debug symbols
are a gigabyte, and the compiler is taking up 6 gigs of memory per process. I guess now I need to split up my
compilation units!

Every time I breathe on my project, I have enough time to go make a coffee. Writing new code comes to a standstill
because the debug/compile loop has a latency of several minutes. I lose focus every time.

This is the basic problem that drove me over the edge: I write SIMD vectorized evaluators for functions that are
dispatched on the number of digits requested. The number of digits is some number from like 3-12. Easy peasy. The
kernels all rely on some expensive polynomial evaluation. Polynomial evaluations are about 2-3x slower if you don't know
the length of the polynomial at compile time because you spend half of your time comparing if `i < N`. The number of
terms can be precomputed in something like matlab, pasted in a big table, and loaded at compile time. Then for each
number of digits, I could just load the coefficients into an array. Except that array is jagged: each digit has a
different number of elements. So I have to wrap my digits into a constexpr lambda rather than an array and return the
array. Not a big deal. Not super annoying. But eh.

But instead, I decided it'd be cool to calculate the polynomials at RUNTIME. This has a nice benefit: If we have a
slightly improved array of coefficients, I can get them automatically. Or I could change the function my polynomial
represents. The test loop is crazy short and I can just play/research.

Then I think: oh shit. I'm going to eventually ship this library to python. This means I need to dispatch on CPU
instructions. Oh, and my kernels would all perform better if I knew which subkernels they should evaluate? Want to use
the calculation while it's live to calculate other things? That's a flag.. and you probably want to know it at compile
time. There are three of these flags. That's 6 more combos. With x86_64 levels, that's another 4. My digits... that's
about 10. My polynomial sizes? That's about 20. Ahh... and float/double... that's another 2. Of course I don't need 12
digits when I'm using float, but now I'd have to write extra instantiation logic to say how many digits are valid if I'm
in a float or a double. KILL ME.


**No one else thinks about this crap**

And they shouldn't. Maybe when I finally ship my library, I'll bother. But when I'm developing, I just need to
develop. Spending a billion years writing all this stuff only to realize there's a smarter way to do it mathematically a
month later and having me re-write all this crazy fancy code is less than ideal. So. If anything, this library can be
used to have performant prototypes when developing. But they shouldn't be that much work. Why should all the other
languages have nice things while we torment ourselves over arcane syntax and feeling clever to write 50 lines of C++ to
get what everyone else gets for free? Sure I get to have a nice puzzle and feel smart and accomplished but that's not
why I code.
