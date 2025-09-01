// Basic unit tests for curve evaluation.
#include "verity/engine.hpp"
#include <cassert>
#include <cmath>
#include <vector>

using namespace verity;

static bool nearly(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // Legacy smoke
    assert(evaluate_curve_sample(3) == 3);

    // Hermite linear should behave like linear interpolation
    int c1 = createCurve(CurveKind::Hermite);
    setKeys(c1, std::vector<Key>{{0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 0.f}});
    assert(nearly(evaluate(c1, 0.f), 0.f));
    assert(nearly(evaluate(c1, 0.5f), 0.5f));
    assert(nearly(evaluate(c1, 1.f), 1.f));

    // Bezier via Hermite-equivalent tangents
    int c2 = createCurve(CurveKind::BezierCubic);
    setKeys(c2, std::vector<Key>{{0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 0.f}});
    float mid_bz = evaluate(c2, 0.5f);
    // Bezier with these tangents should be monotonic from 0..1 and close to 0.5
    assert(mid_bz > 0.3f && mid_bz < 0.7f);

    // Catmull-Rom (centripetal) through 0, 1, 0
    int c3 = createCurve(CurveKind::CatmullRom);
    setKeys(c3, std::vector<Key>{{0.f, 0.f, 0.f, 0.f}, {0.5f, 1.f, 0.f, 0.f}, {1.f, 0.f, 0.f, 0.f}});
    float v0 = evaluate(c3, 0.f);
    float v05 = evaluate(c3, 0.5f);
    float v1 = evaluate(c3, 1.f);
    assert(nearly(v0, 0.f));
    assert(v05 > 0.7f); // peak near middle
    assert(nearly(v1, 0.f));

    // Constant-speed remap reduces variance of delta values along curve
    int c4 = createCurve(CurveKind::Hermite);
    setKeys(c4, std::vector<Key>{{0.f, 0.f, 0.f, 0.f}, {1.f, 1.f, 0.f, 0.f}}); // ease-in/out without tangents
    const int N = 50;
    float prev = evaluate(c4, 0.f);
    float sum = 0.f, sum2 = 0.f;
    for (int i = 1; i <= N; ++i) {
        float t = float(i) / float(N);
        float v = evaluate(c4, t);
        float d = std::fabs(v - prev);
        sum += d; sum2 += d * d; prev = v;
    }
    float mean = sum / N;
    float var = sum2 / N - mean * mean;

    setConstantSpeed(c4, true);
    prev = evaluate(c4, 0.f);
    float sum_cs = 0.f, sum2_cs = 0.f;
    for (int i = 1; i <= N; ++i) {
        float t = float(i) / float(N);
        float v = evaluate(c4, t);
        float d = std::fabs(v - prev);
        sum_cs += d; sum2_cs += d * d; prev = v;
    }
    float mean_cs = sum_cs / N;
    float var_cs = sum2_cs / N - mean_cs * mean_cs;
    assert(var_cs <= var); // should not be worse than linear u

    // Blend sanity
    int ca = createCurve(CurveKind::Hermite);
    setKeys(ca, std::vector<Key>{{0.f, 0.f, 0.f, 0.f}, {1.f, 0.f, 0.f, 0.f}});
    int cb = createCurve(CurveKind::Hermite);
    setKeys(cb, std::vector<Key>{{0.f, 1.f, 0.f, 0.f}, {1.f, 1.f, 0.f, 0.f}});
    assert(nearly(evaluateBlended(ca, cb, 0.25f, 0.33f), 0.25f));

    return 0;
}
