#pragma once
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <omp.h>

class FluidSolver {
public:
    int N;
    float dt;
    float visc;

    FluidSolver(int n);

    int IX(int i, int j) const;
    void addDensity(int x, int y, float amount, float radius);
    void addDensityLine(int x1, int y1, int x2, int y2, float amount, float radius);
    void addVelocity(int x, int y, float vx, float vy, float radius);
    void addSplash(int cx, int cy, float intensity);
    void step(float gravity);
    void reset();

    std::vector<float> u, v, u_prev, v_prev,
                       dens, dens_prev,
                       p, div, wk;

private:
    void setBnd(float* f, int b);
    float lerp(float a, float b, float t);
    float sample(float* f, float x, float y);
    void advect(float* d, float* d0, float* u, float* v, float dt, bool reflect = false);
    void diffuse(float* x, float* x0, float diff, float dt);
    void project(float* u, float* v, float* p, float* div);
};
