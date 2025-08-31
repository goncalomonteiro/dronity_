#include "verity/engine.hpp"

namespace verity {

int evaluate_curve_sample(int n) {
    // trivial function to be exercised by tests and clang-tidy
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        acc += i;
    }
    return acc;
}

} // namespace verity

