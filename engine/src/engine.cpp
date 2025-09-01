#include "verity/engine.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace verity {

namespace {

struct SegmentLUT {
    // Uniform u samples in [0,1], cumulative arc-length s(u)
    std::vector<float> u;
    std::vector<float> s;
    float total = 0.f;
};

struct Curve {
    CurveKind kind {CurveKind::Hermite};
    std::vector<Key> keys;
    bool constantSpeed {false};
    // One LUT per segment (keys.size()-1)
    std::vector<SegmentLUT> luts;
};

static std::vector<Curve> g_curves;

inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

// Hermite basis evaluation for scalar value
static inline float hermite(float p0, float p1, float m0, float m1, float u) {
    float u2 = u * u;
    float u3 = u2 * u;
    float h00 = 2 * u3 - 3 * u2 + 1;
    float h10 = u3 - 2 * u2 + u;
    float h01 = -2 * u3 + 3 * u2;
    float h11 = u3 - u2;
    return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
}

static inline float bezier_from_hermite(float p0, float p1, float m0, float m1, float u) {
    // Convert Hermite form to Bezier control points and evaluate
    // For scalar: C0=p0, C3=p1, C1=p0 + m0/3, C2=p1 - m1/3
    float c0 = p0;
    float c1 = p0 + m0 / 3.f;
    float c2 = p1 - m1 / 3.f;
    float c3 = p1;
    float one = 1.f - u;
    return one * one * one * c0 + 3.f * one * one * u * c1 + 3.f * one * u * u * c2 + u * u * u * c3;
}

static inline float catmull_rom(float p_1, float p0, float p1, float p2, float u, float tau = 0.5f) {
    // Standard Catmull-Rom with tension tau (0.5 is centripetal)
    float m0 = tau * (p1 - p_1);
    float m1 = tau * (p2 - p0);
    return hermite(p0, p1, m0, m1, u);
}

// dv/du for Hermite (approx for LUT). Here we use small delta for numerical derivative.
static inline float eval_segment(CurveKind kind,
                                 const std::vector<Key>& keys,
                                 size_t i,
                                 float u) {
    const Key& k0 = keys[i];
    const Key& k1 = keys[i + 1];
    switch (kind) {
    case CurveKind::Hermite: {
        float dt = (k1.time - k0.time);
        float m0 = k0.outTan * dt;
        float m1 = k1.inTan * dt;
        return hermite(k0.value, k1.value, m0, m1, u);
    }
    case CurveKind::BezierCubic: {
        float dt = (k1.time - k0.time);
        float m0 = k0.outTan * dt;
        float m1 = k1.inTan * dt;
        return bezier_from_hermite(k0.value, k1.value, m0, m1, u);
    }
    case CurveKind::CatmullRom: {
        float p_1 = (i == 0) ? keys[i].value : keys[i - 1].value;
        float p2 = (i + 2 < keys.size()) ? keys[i + 2].value : keys[i + 1].value;
        return catmull_rom(p_1, k0.value, k1.value, p2, u, 0.5f);
    }
    }
    return 0.f;
}

static SegmentLUT build_lut(const Curve& c, size_t segIndex, int samples = 64) {
    SegmentLUT out;
    out.u.resize(samples + 1);
    out.s.resize(samples + 1);
    out.u[0] = 0.f;
    out.s[0] = 0.f;
    float prev = eval_segment(c.kind, c.keys, segIndex, 0.f);
    float accum = 0.f;
    for (int i = 1; i <= samples; ++i) {
        float u = float(i) / float(samples);
        float v = eval_segment(c.kind, c.keys, segIndex, u);
        // arc length in value-space along u; approximate via |delta v|
        accum += std::abs(v - prev);
        prev = v;
        out.u[i] = u;
        out.s[i] = accum;
    }
    out.total = accum;
    if (out.total <= 1e-6f) {
        // avoid zero-length
        out.total = 1e-6f;
        for (auto& s : out.s) s = 0.f;
    }
    return out;
}

static void rebuild_luts(Curve& c) {
    c.luts.clear();
    if (c.keys.size() < 2) return;
    c.luts.reserve(c.keys.size() - 1);
    for (size_t i = 0; i + 1 < c.keys.size(); ++i) {
        c.luts.emplace_back(build_lut(c, i, 64));
    }
}

static float remap_u_by_arclength(const SegmentLUT& lut, float u_linear) {
    float target = lut.total * clamp01(u_linear);
    // find smallest j with s[j] >= target
    const auto& s = lut.s;
    const auto& uu = lut.u;
    auto it = std::lower_bound(s.begin(), s.end(), target);
    if (it == s.begin()) return uu.front();
    if (it == s.end()) return uu.back();
    size_t j = size_t(it - s.begin());
    float s1 = s[j - 1], s2 = s[j];
    float u1 = uu[j - 1], u2 = uu[j];
    float t = (target - s1) / std::max(1e-6f, (s2 - s1));
    return u1 + t * (u2 - u1);
}

} // namespace

int createCurve(CurveKind kind) {
    Curve c;
    c.kind = kind;
    int id = static_cast<int>(g_curves.size());
    g_curves.emplace_back(std::move(c));
    return id;
}

void setKeys(int curveId, const std::vector<Key>& keys) {
    if (curveId < 0 || static_cast<size_t>(curveId) >= g_curves.size()) throw std::out_of_range("curveId");
    if (keys.size() < 2) throw std::invalid_argument("setKeys requires at least two keys");
    auto& c = g_curves[static_cast<size_t>(curveId)];
    c.keys = keys;
    // ensure sorted by time
    std::sort(c.keys.begin(), c.keys.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
    if (c.constantSpeed) rebuild_luts(c);
}

void setConstantSpeed(int curveId, bool enabled) {
    if (curveId < 0 || static_cast<size_t>(curveId) >= g_curves.size()) throw std::out_of_range("curveId");
    auto& c = g_curves[static_cast<size_t>(curveId)];
    c.constantSpeed = enabled;
    if (enabled) rebuild_luts(c);
}

static inline size_t find_segment(const std::vector<Key>& keys, float time) {
    if (time <= keys.front().time) return 0;
    if (time >= keys.back().time) return keys.size() - 2;
    size_t lo = 0, hi = keys.size() - 1;
    while (lo + 1 < hi) {
        size_t mid = (lo + hi) / 2;
        if (time < keys[mid].time) hi = mid; else lo = mid;
    }
    return lo;
}

float evaluate(int curveId, float time) {
    if (curveId < 0 || static_cast<size_t>(curveId) >= g_curves.size()) throw std::out_of_range("curveId");
    const auto& c = g_curves[static_cast<size_t>(curveId)];
    if (c.keys.size() < 2) return 0.f;
    size_t i = find_segment(c.keys, time);
    const Key& k0 = c.keys[i];
    const Key& k1 = c.keys[i + 1];
    float u = (time - k0.time) / std::max(1e-6f, (k1.time - k0.time));
    u = clamp01(u);
    if (c.constantSpeed && i < c.luts.size()) {
        u = remap_u_by_arclength(c.luts[i], u);
    }
    return eval_segment(c.kind, c.keys, i, u);
}

float evaluateBlended(int curveA, int curveB, float alpha, float time) {
    float a = evaluate(curveA, time);
    float b = evaluate(curveB, time);
    return a * (1.f - alpha) + b * alpha;
}

int evaluate_curve_sample(int n) {
    int acc = 0;
    for (int i = 0; i < n; ++i) acc += i;
    return acc;
}

} // namespace verity
