#include "verity/engine.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

using namespace verity;

int main() {
    // Build 10k-key Hermite curve approximating a sine wave on [0, 10]
    const int K = 10000;
    std::vector<Key> keys;
    keys.reserve(K);
    for (int i = 0; i < K; ++i) {
        float t = 10.f * (float(i) / float(K - 1));
        float v = std::sin(t);
        // Approximate slope dv/dt of sine = cos(t)
        float m = std::cos(t);
        keys.push_back(Key{t, v, m, m});
    }

    int id = createCurve(CurveKind::Hermite);
    setKeys(id, keys);
    setConstantSpeed(id, true); // enable LUT path

    // Evaluate at N sample times and measure wall time
    const int N = 200000; // 200k evaluations
    volatile float sink = 0.f; // prevent optimizing away
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        float x = 10.f * (float(i) / float(N - 1));
        sink += evaluate(id, x);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double per_eval_ns = double(ns) / double(N);
    double total_ms = double(ns) / 1e6;
    std::cout << "evals=" << N << ", keys=" << K << ", total_ms=" << total_ms
              << ", per_eval_ns=" << per_eval_ns << "\n";
    // Print sink to avoid optimizing away
    std::cerr << "sink=" << sink << "\n";
    return 0;
}

