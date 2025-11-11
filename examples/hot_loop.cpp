void hot_loop(float *arr, int N) {
    arr = (float *)__builtin_assume_aligned(arr, 64);
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2.0f;
    }
}

void hot_loop_const(float *arr) {
    arr = (float *)__builtin_assume_aligned(arr, 64);
    int N = 1;
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2.0f;
    }
}

template <typename T>
void hot_loop_template(T *arr, int N) {
    arr = (T *)__builtin_assume_aligned(arr, 64);
    for (int i = 0; i < N; ++i) {
        arr[i] = arr[i] * 2;
    }
}

template void hot_loop_template<float>(float *arr, int N);
template void hot_loop_template<double>(double *arr, int N);
