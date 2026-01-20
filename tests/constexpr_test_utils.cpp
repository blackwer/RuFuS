/// @brief Checks if a number is even or odd based on the check_even flag
bool is_even_or_odd(int x, bool check_even) {
  if (check_even) {
    return x % 2 == 0;
  } else {
    return x % 2 != 0;
  }
}

/// @brief Template version to check if a number is even or odd
template<bool check_even>
bool is_even_or_odd_template(int x) {
  if (check_even) {
    return x % 2 == 0;
  } else {
    return x % 2 != 0;
  }
}
//
template bool is_even_or_odd_template<true>(int x);
template bool is_even_or_odd_template<false>(int x);

/// @brief Constexpr version to check if a number is even or odd
template<bool check_even>
bool is_even_or_odd_constexpr(int x) {
  if constexpr (check_even) {
    return x % 2 == 0;
  } else {
    return x % 2 != 0;
  }
}
//
template bool is_even_or_odd_constexpr<true>(int x);
template bool is_even_or_odd_constexpr<false>(int x);
