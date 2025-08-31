#include <cassert>
#include "verity/engine.hpp"

int main() {
    // Sum 0..3 = 0+1+2 = 3
    int v = verity::evaluate_curve_sample(3);
    assert(v == 3);
    return 0;
}

