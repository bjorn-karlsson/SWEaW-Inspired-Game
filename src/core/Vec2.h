#pragma once

#include <cmath>

namespace gc {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }

    Vec2 normalized() const {
        float len = length();
        if (len <= 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }
};

inline float distance(const Vec2& a, const Vec2& b) { return (a - b).length(); }
inline float distanceSq(const Vec2& a, const Vec2& b) { return (a - b).lengthSq(); }
inline Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return a + (b - a) * t; }

}  // namespace gc
