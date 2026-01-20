/*
Constexpr functions to be specialized by RuFuS

The goal is to write a test like

RuFuS RS;
RS.load_ir_string(rufus::embedded::constexpr_functions_ir)
   .specialize_function("is_even_or_odd(int,bool)", {{"check_even", true}})"
   .optimize();

*/

bool is_even_or_odd(int x, bool check_even) {
  if constexpr (check_even) {
    return x % 2 == 0;
  } else {
    return x % 2 != 0;
  }
}
