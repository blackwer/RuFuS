#include "accessor.hpp"
#include <cstddef>
#include <variant>

void axpby(double a, const Accessor &x, double b, const Accessor &y, std::size_t N) {
    for (std::size_t i = 0; i < N; ++i) {
        y(i) = a * x(i) + b * y(i);
    }
}

using vector_or_scalar = std::variant<double, std::vector<double>>;
void axpby(double a, const vector_or_scalar &x, double b, vector_or_scalar &y, std::size_t N, bool is_x_shared,
           bool is_y_shared) {
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

void axpby_explicit_branches(double a, const vector_or_scalar &x, double b, vector_or_scalar &y, std::size_t N,
                             bool is_x_shared, bool is_y_shared) {
    if (is_x_shared && is_y_shared) {
        for (std::size_t i = 0; i < N; ++i) {
            double x_val = std::get<double>(x);
            double y_val = std::get<double>(y);
            double result = a * x_val + b * y_val;
            std::get<double>(y) = result;
        }
    } else if (is_x_shared && !is_y_shared) {
        for (std::size_t i = 0; i < N; ++i) {
            double x_val = std::get<double>(x);
            double y_val = std::get<std::vector<double>>(y)[i];
            double result = a * x_val + b * y_val;
            std::get<std::vector<double>>(y)[i] = result;
        }
    } else if (!is_x_shared && is_y_shared) {
        for (std::size_t i = 0; i < N; ++i) {
            double x_val = std::get<std::vector<double>>(x)[i];
            double y_val = std::get<double>(y);
            double result = a * x_val + b * y_val;
            std::get<double>(y) = result;
        }
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            double x_val = std::get<std::vector<double>>(x)[i];
            double y_val = std::get<std::vector<double>>(y)[i];
            double result = a * x_val + b * y_val;
            std::get<std::vector<double>>(y)[i] = result;
        }
    }
}