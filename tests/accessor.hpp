#ifndef TESTS_ACCESSOR_HPP_
#define TESTS_ACCESSOR_HPP_

#include <vector>

template <typename T>
struct NonOwningVector {
    // Just a pointer and a size
    T *data = nullptr;
    std::size_t size = 0;

    inline NonOwningVector() = default;

    inline NonOwningVector(T *data_ptr, std::size_t sz) : data(data_ptr), size(sz) {}

    inline NonOwningVector(std::vector<T> &vec) : data(vec.data()), size(vec.size()) {}

    // Copy/move constructors and assignments
    inline NonOwningVector(const NonOwningVector &other) = default;
    inline NonOwningVector(NonOwningVector &&other) = default;
    inline NonOwningVector &operator=(const NonOwningVector &other) = default;
    inline NonOwningVector &operator=(NonOwningVector &&other) = default;

    inline T &operator[](std::size_t i) const { return data[i]; }
};

class Accessor {
  public:
    Accessor() = default;

    explicit inline Accessor(double shared_value) : shared_value_(shared_value), vector_{}, is_shared_(true) {}

    explicit inline Accessor(std::vector<double> &vec)
        : shared_value_{}, vector_(NonOwningVector<double>(vec)), is_shared_(false) {}

    inline double &operator()(std::size_t i) const {
        return is_shared_ ? const_cast<double &>(shared_value_) : const_cast<double &>(vector_[i]);
    }

    const double shared_value_ = 0.0;
    const NonOwningVector<double> vector_;
    const bool is_shared_ = false;
};

#endif // TESTS_ACCESSOR_HPP_
