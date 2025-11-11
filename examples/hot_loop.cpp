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
