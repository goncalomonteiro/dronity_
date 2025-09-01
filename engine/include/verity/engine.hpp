#pragma once

#include <cstdint>
#include <vector>

namespace verity {

enum class CurveKind : uint8_t {
    BezierCubic = 0,
    Hermite = 1,
    CatmullRom = 2,
};

struct Key {
    float time;   // milliseconds or normalized seconds
    float value;  // scalar value (position component)
    float inTan;  // incoming slope (for Bezier/Hermite)
    float outTan; // outgoing slope (for Bezier/Hermite)
};

// Creates a curve and returns its integer id. The engine holds the curve.
int createCurve(CurveKind kind);

// Replace keys for a curve (keys must be sorted by time and contain at least 2 entries).
void setKeys(int curveId, const std::vector<Key>& keys);

// Enable/disable constant-speed evaluation using an arc-length LUT per segment.
void setConstantSpeed(int curveId, bool enabled);

// Evaluate curve at absolute time (uses key times for segment selection).
float evaluate(int curveId, float time);

// Evaluate linear blend of two curves at time.
float evaluateBlended(int curveA, int curveB, float alpha, float time);

// Simple helper kept for legacy test; sums 0..n-1
int evaluate_curve_sample(int n);

} // namespace verity
