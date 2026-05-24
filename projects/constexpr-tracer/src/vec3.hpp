#pragma once
#include <cstdint>

// ─── Constexpr math helpers ──────────────────────────────────────────────────
constexpr float csqrt(float x) {
    if (x <= 0) return 0;
    float s = x;
    for (int i = 0; i < 15; i++) s = 0.5f * (s + x / s);
    return s;
}

constexpr float cpow(float x, int n) {
    float r = 1;
    for (int i = 0; i < n; i++) r *= x;
    return r;
}

constexpr float clamp(float x, float lo, float hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

// ─── 3D vector ───────────────────────────────────────────────────────────────
struct Vec3 {
    float x, y, z;

    constexpr Vec3() : x(0), y(0), z(0) {}
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    constexpr Vec3 operator+(Vec3 v) const { return {x+v.x, y+v.y, z+v.z}; }
    constexpr Vec3 operator-(Vec3 v) const { return {x-v.x, y-v.y, z-v.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    constexpr Vec3 operator/(float s) const { float inv = 1.0f/s; return {x*inv, y*inv, z*inv}; }

    constexpr Vec3& operator+=(Vec3 v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    constexpr Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }

    constexpr float dot(Vec3 v) const { return x*v.x + y*v.y + z*v.z; }
    constexpr Vec3 cross(Vec3 v) const { return {y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x}; }
    constexpr float len2() const { return dot(*this); }
    constexpr Vec3 norm() const { float l = csqrt(len2()); return l > 0 ? *this / l : *this; }
};

constexpr Vec3 clamp(Vec3 v, float lo, float hi) {
    return {clamp(v.x, lo, hi), clamp(v.y, lo, hi), clamp(v.z, lo, hi)};
}
