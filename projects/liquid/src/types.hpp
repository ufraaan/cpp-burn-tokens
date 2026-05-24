#pragma once
#include <cmath>

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2 operator+(Vec2 o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(Vec2 o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(float s) const { return {x*s, y*s}; }
    Vec2 operator/(float s) const { return {x/s, y/s}; }
    Vec2& operator+=(Vec2 o) { x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x-=o.x; y-=o.y; return *this; }
    float dot(Vec2 o) const { return x*o.x + y*o.y; }
    float len2() const { return x*x + y*y; }
    float len() const { return std::sqrt(len2()); }
    Vec2 norm() const { float l = len(); return l > 0 ? *this / l : Vec2{0,0}; }
};

struct Particle {
    Vec2 pos;
    Vec2 vel;
    float density;
    float pressure;
    Particle() : pos(), vel(), density(0), pressure(0) {}
    Particle(float x, float y) : pos(x, y), vel(), density(0), pressure(0) {}
};
