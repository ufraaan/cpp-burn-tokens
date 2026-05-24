#include "fluid.hpp"

FluidSolver::FluidSolver(int n)
    : N(n), dt(0.02f), visc(0.0005f)
{
    int sz = N + 2;
    u.assign(sz * sz, 0.0f);
    v.assign(sz * sz, 0.0f);
    u_prev.assign(sz * sz, 0.0f);
    v_prev.assign(sz * sz, 0.0f);
    dens.assign(sz * sz, 0.0f);
    dens_prev.assign(sz * sz, 0.0f);
    dens_r.assign(sz * sz, 0.0f);
    dens_r_prev.assign(sz * sz, 0.0f);
    dens_b.assign(sz * sz, 0.0f);
    dens_b_prev.assign(sz * sz, 0.0f);
    p.assign(sz * sz, 0.0f);
    div.assign(sz * sz, 0.0f);
    wk.assign(sz * sz, 0.0f);
}

int FluidSolver::IX(int i, int j) const { return j * (N + 2) + i; }

void FluidSolver::addDensityLine(int x1, int y1, int x2, int y2, float amount, float radius, bool is_red) {
    int steps = std::max(std::abs(x2 - x1), std::abs(y2 - y1));
    if (steps < 1) steps = 1;
    for (int s = 0; s <= steps; s++) {
        float t = (float)s / steps;
        int cx = x1 + (int)((x2 - x1) * t);
        int cy = y1 + (int)((y2 - y1) * t);
        addDensity(cx, cy, amount, radius, is_red);
    }
}

void FluidSolver::addDensity(int x, int y, float amount, float radius, bool is_red) {
    float* dst = is_red ? dens_r.data() : dens_b.data();
    for (int j = -(int)radius; j <= (int)radius; j++) {
        for (int i = -(int)radius; i <= (int)radius; i++) {
            int gx = x + i, gy = y + j;
            if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
            float d = std::sqrt(i*i + j*j);
            if (d > radius) continue;
            float w = amount * (1 - d / radius);
            dst[IX(gx, gy)] += w;
            dens[IX(gx, gy)] += w;
        }
    }
}

void FluidSolver::addVelocity(int x, int y, float vx, float vy, float radius) {
    for (int j = -(int)radius; j <= (int)radius; j++) {
        for (int i = -(int)radius; i <= (int)radius; i++) {
            int gx = x + i, gy = y + j;
            if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
            float d = std::sqrt(i*i + j*j);
            if (d > radius) continue;
            float w = 1 - d / radius;
            u[IX(gx, gy)] += vx * w;
            v[IX(gx, gy)] += vy * w;
        }
    }
}

void FluidSolver::addSplash(int cx, int cy, float intensity) {
    int r = 15;
    addDensity(cx, cy, intensity, r, false);
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            int gx = cx + i, gy = cy + j;
            if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
            float dist = std::sqrt(i*i + j*j);
            if (dist > r || dist < 1.0f) continue;
            float strength = 60.0f * (1.0f - dist / r);
            float angle = std::atan2(j - 3, i) + ((rand() % 200) - 100) * 0.008f;
            u[IX(gx, gy)] += std::cos(angle) * strength;
            v[IX(gx, gy)] += std::sin(angle) * strength;
            if (dist > r * 0.35f && (rand() % 4 == 0)) {
                float sw = 0.4f * (1.0f - dist / r);
                dens_b[IX(gx, gy)] += sw;
                dens[IX(gx, gy)] += sw;
            }
        }
    }
}

void FluidSolver::addVorticityConfinement(float eps) {
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++)
        for (int i = 1; i <= N; i++)
            wk[IX(i, j)] = (v[IX(i+1, j)] - v[IX(i-1, j)]
                          - u[IX(i, j+1)] + u[IX(i, j-1)]) * 0.5f * N;
    #pragma omp parallel for collapse(2)
    for (int j = 2; j < N; j++)
        for (int i = 2; i < N; i++) {
            float dwdx = (std::abs(wk[IX(i+1, j)]) - std::abs(wk[IX(i-1, j)])) * 0.5f * N;
            float dwdy = (std::abs(wk[IX(i, j+1)]) - std::abs(wk[IX(i, j-1)])) * 0.5f * N;
            float len = std::sqrt(dwdx * dwdx + dwdy * dwdy) + 1e-8f;
            dwdx /= len; dwdy /= len;
            u[IX(i, j)] += eps * dwdy * wk[IX(i, j)];
            v[IX(i, j)] -= eps * dwdx * wk[IX(i, j)];
        }
}

void FluidSolver::step(float gravity, float vort_eps) {
    // 1. Vorticity confinement
    addVorticityConfinement(vort_eps);

    // 2. Gravity — only where water exists
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++)
        for (int i = 1; i <= N; i++)
            v[IX(i, j)] += gravity * dt * std::min(dens[IX(i, j)], 1.0f);

    // 3. Advect velocity
    advect(u_prev.data(), u.data(), u.data(), v.data(), dt);
    advect(v_prev.data(), v.data(), u.data(), v.data(), dt);
    std::swap(u, u_prev);
    std::swap(v, v_prev);

    // 4. Diffuse velocity
    diffuse(u_prev.data(), u.data(), visc, dt);
    diffuse(v_prev.data(), v.data(), visc, dt);
    std::swap(u, u_prev);
    std::swap(v, v_prev);

    // 5. Project
    project(u.data(), v.data(), p.data(), div.data());

    // 6. Advect density with reflective walls
    advect(dens_prev.data(), dens.data(), u.data(), v.data(), dt, true);
    std::swap(dens, dens_prev);
    setBnd(dens.data(), 0);

    // 7. Advect red and blue dye
    advect(dens_r_prev.data(), dens_r.data(), u.data(), v.data(), dt, true);
    std::swap(dens_r, dens_r_prev);
    setBnd(dens_r.data(), 0);
    advect(dens_b_prev.data(), dens_b.data(), u.data(), v.data(), dt, true);
    std::swap(dens_b, dens_b_prev);
    setBnd(dens_b.data(), 0);

    // 8. Cap densities
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++)
        for (int i = 1; i <= N; i++)
            dens[IX(i, j)] = std::min(dens[IX(i, j)], 1.0f);
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++)
        for (int i = 1; i <= N; i++) {
            dens_r[IX(i, j)] = std::min(dens_r[IX(i, j)], 1.0f);
            dens_b[IX(i, j)] = std::min(dens_b[IX(i, j)], 1.0f);
        }
}

void FluidSolver::reset() {
    std::fill(u.begin(), u.end(), 0.0f);
    std::fill(v.begin(), v.end(), 0.0f);
    std::fill(dens.begin(), dens.end(), 0.0f);
    std::fill(dens_r.begin(), dens_r.end(), 0.0f);
    std::fill(dens_b.begin(), dens_b.end(), 0.0f);
    std::fill(u_prev.begin(), u_prev.end(), 0.0f);
    std::fill(v_prev.begin(), v_prev.end(), 0.0f);
    std::fill(dens_prev.begin(), dens_prev.end(), 0.0f);
    std::fill(dens_r_prev.begin(), dens_r_prev.end(), 0.0f);
    std::fill(dens_b_prev.begin(), dens_b_prev.end(), 0.0f);
}

void FluidSolver::setBnd(float* f, int b) {
    for (int i = 1; i <= N; i++) {
        f[IX(i, 0)]   = (b == 2) ? -f[IX(i, 1)]   : f[IX(i, 1)];
        f[IX(i, N+1)] = (b == 2) ? -f[IX(i, N)]   : f[IX(i, N)];
    }
    for (int j = 1; j <= N; j++) {
        f[IX(0, j)]   = (b == 1) ? -f[IX(1, j)]   : f[IX(1, j)];
        f[IX(N+1, j)] = (b == 1) ? -f[IX(N, j)]   : f[IX(N, j)];
    }
    f[IX(0, 0)]     = (f[IX(1, 0)] + f[IX(0, 1)]) * 0.5f;
    f[IX(N+1, 0)]   = (f[IX(N, 0)] + f[IX(N+1, 1)]) * 0.5f;
    f[IX(0, N+1)]   = (f[IX(1, N+1)] + f[IX(0, N)]) * 0.5f;
    f[IX(N+1, N+1)] = (f[IX(N, N+1)] + f[IX(N+1, N)]) * 0.5f;
}

float FluidSolver::lerp(float a, float b, float t) { return a + t * (b - a); }

float FluidSolver::sample(float* f, float x, float y) {
    int i = (int)x, j = (int)y;
    float fx = x - i, fy = y - j;
    i = std::max(1, std::min(N, i));
    j = std::max(1, std::min(N, j));
    int ni = std::min(i + 1, N);
    int nj = std::min(j + 1, N);
    return lerp(lerp(f[IX(i, j)], f[IX(ni, j)], fx),
                lerp(f[IX(i, nj)], f[IX(ni, nj)], fx), fy);
}

void FluidSolver::advect(float* d, float* d0, float* u, float* v, float dt, bool reflect) {
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++) {
        for (int i = 1; i <= N; i++) {
            float x = i - dt * u[IX(i, j)];
            float y = j - dt * v[IX(i, j)];
            if (reflect) {
                for (int it = 0; it < 8; it++) {
                    bool ok = true;
                    if (x < 0.5f) { x = 1.0f - x; ok = false; }
                    else if (x > N + 0.5f) { x = 2.0f * (N + 0.5f) - x; ok = false; }
                    if (y < 0.5f) { y = 1.0f - y; ok = false; }
                    else if (y > N + 0.5f) { y = 2.0f * (N + 0.5f) - y; ok = false; }
                    if (ok) break;
                }
                x = std::max(0.5f, std::min(x, (float)N + 0.5f));
                y = std::max(0.5f, std::min(y, (float)N + 0.5f));
            } else {
                if (x < 0.5f) x = 0.5f;
                if (x > N + 0.5f) x = N + 0.5f;
                if (y < 0.5f) y = 0.5f;
                if (y > N + 0.5f) y = N + 0.5f;
            }
            d[IX(i, j)] = sample(d0, x, y);
        }
    }
}

void FluidSolver::diffuse(float* x, float* x0, float diff, float dt) {
    float a = dt * diff * N * N;
    for (int k = 0; k < 20; k++) {
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                wk[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i-1, j)] + x[IX(i+1, j)]
                                                   + x[IX(i, j-1)] + x[IX(i, j+1)]))
                             / (1 + 4 * a);
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                x[IX(i, j)] = wk[IX(i, j)];
        setBnd(x, 1);
    }
}

void FluidSolver::project(float* u, float* v, float* p, float* div) {
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++) {
        for (int i = 1; i <= N; i++) {
            div[IX(i, j)] = (u[IX(i+1, j)] - u[IX(i-1, j)]
                           + v[IX(i, j+1)] - v[IX(i, j-1)]) * (-0.5f / N);
            p[IX(i, j)] = 0;
        }
    }
    setBnd(div, 0);
    for (int k = 0; k < 30; k++) {
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                wk[IX(i, j)] = (div[IX(i, j)]
                             + p[IX(i-1, j)] + p[IX(i+1, j)]
                             + p[IX(i, j-1)] + p[IX(i, j+1)]) * 0.25f;
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                p[IX(i, j)] = wk[IX(i, j)];
        setBnd(p, 0);
    }
    #pragma omp parallel for collapse(2)
    for (int j = 1; j <= N; j++) {
        for (int i = 1; i <= N; i++) {
            u[IX(i, j)] -= 0.5f * N * (p[IX(i+1, j)] - p[IX(i-1, j)]);
            v[IX(i, j)] -= 0.5f * N * (p[IX(i, j+1)] - p[IX(i, j-1)]);
        }
    }
    setBnd(u, 1);
    setBnd(v, 2);
}
