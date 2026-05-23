#pragma once
#include <cmath>
#include <vector>

struct Vec2 {
    double x = 0, y = 0;
    Vec2() = default;
    Vec2(double x, double y) : x(x), y(y) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double s) const { return {x * s, y * s}; }
    Vec2 operator/(double s) const { return {x / s, y / s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    double len2() const { return x * x + y * y; }
    double len() const { return std::sqrt(len2()); }
};

static inline double clamp(double v, double lo, double hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

struct BlackHole {
    Vec2 pos, vel;
    double mass = 10.0;
    double radius = 3.0;
    bool merged = false;
    double mergeProgress = 0.0;
    std::vector<Vec2> trail;
    static constexpr int MAX_TRAIL = 120;
};

struct Star {
    Vec2 pos;
    double brightness;
    double twinklePhase;
    int colorIdx;
};

struct Particle {
    Vec2 pos, vel;
    double lifetime, maxLife;
    int r, g, b, a;
};

struct GridPoint {
    Vec2 origPos;
    Vec2 distortPos;
};

struct GravWave {
    Vec2 origin;
    double startTime;
    double amplitude;
};

struct SimulationConfig {
    double G = 500.0;
    double DT = 0.005;
    int substeps = 5;
    int numStars = 600;
    int maxParticles = 2500;
    int gridSpacing = 3;
    int gridExtent = 120;
};
