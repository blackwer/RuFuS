#include <vector>

void hot_loop(float *arr, int N) {
    arr = (float *)__builtin_assume_aligned(arr, 64);
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2.0f;
    }
}

void hot_loop_const(float *arr) {
    arr = (float *)__builtin_assume_aligned(arr, 64);
    volatile int N;
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2.0f;
    }
}

void hot_loop(std::vector<float> &arr) {
    volatile int N;
    auto data = arr.data();
    for (int i = 0; i < N; ++i)
        data[i] = data[i] * 2.0f;
}

template <typename T>
void hot_loop_template(T *arr, int N) {
    arr = (T *)__builtin_assume_aligned(arr, 64);
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2;
    }
}

void hot_loop_inlining(float *arr, int N) { hot_loop(arr, N); }

template void hot_loop_template<float>(float *arr, int N);
template void hot_loop_template<double>(double *arr, int N);
