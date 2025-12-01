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

template <class Evaluator>
void evaluate_all_pairs(float *__restrict__ rs, float *__restrict__ rt, float *__restrict__ u, int Nsrc, int Ntrg, Evaluator eval) {
    for (int j = 0; j < Nsrc; ++j) {
        for (int i = 0; i < Ntrg; ++i) {
            u[i] += eval(rs + j * 3, rt + i * 3);
        }
    }
}

template <class Evaluator>
void evaluate_all_pairs(float *__restrict__ rs, float *__restrict__ rt, float *__restrict__ u, int Nsrc, int Ntrg) {
    evaluate_all_pairs(rs, rt, u, Nsrc, Ntrg, Evaluator{});
}

struct InvR2Evaluator {
    float operator()(float *__restrict__ rs, float *__restrict__ rt) const {
        float dx = rt[0] - rs[0];
        float dy = rt[1] - rs[1];
        float dz = rt[2] - rs[2];
        float r2 = dx * dx + dy * dy + dz * dz;
        return 1.0f / r2;
    }
};

void evaluate_all_pairs_inv_r2_struct(float *__restrict__ rs, float *__restrict__ rt, float *__restrict__ u, int Nsrc,
                                      int Ntrg) {
    evaluate_all_pairs<InvR2Evaluator>(rs, rt, u, Nsrc, Ntrg);
}

void evaluate_all_pairs_inv_r2_lambda(float *__restrict__ rs, float *__restrict__ rt, float *__restrict__ u, int Nsrc,
                                      int Ntrg) {
    evaluate_all_pairs(rs, rt, u, Nsrc, Ntrg, [](float *__restrict__ rs, float *__restrict__ rt) {
        float dx = rt[0] - rs[0];
        float dy = rt[1] - rs[1];
        float dz = rt[2] - rs[2];
        float r2 = dx * dx + dy * dy + dz * dz;
        // float r = std::sqrt(r2);
        return 1.0f / r2;
    });
}
