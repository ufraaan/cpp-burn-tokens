#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>
#include <algorithm>

// ─── 2D Navier-Stokes grid solver ───────────────────────────────────────────
// MAC grid: velocities at cell faces, pressure/density at cell centers
// Interior: cells 1..N (0 and N+1 are boundary/ghost)

class FluidSolver {
public:
    int N;          // grid cells per dimension
    float cellSize; // pixels per cell
    float dt;
    float visc;
    float densitySrc;

    FluidSolver(int n, float cs)
        : N(n), cellSize(cs), dt(0.016f), visc(0.0005f), densitySrc(0.5f)
    {
        int sz = N + 2;
        u.assign(sz * sz, 0.0f);
        v.assign(sz * sz, 0.0f);
        u_prev.assign(sz * sz, 0.0f);
        v_prev.assign(sz * sz, 0.0f);
        dens.assign(sz * sz, 0.0f);
        dens_prev.assign(sz * sz, 0.0f);
        p.assign(sz * sz, 0.0f);
        div.assign(sz * sz, 0.0f);
    }

    void addDensity(int x, int y, float amount, float radius) {
        int sz = N + 2;
        for (int j = -radius; j <= radius; j++) {
            for (int i = -radius; i <= radius; i++) {
                int gx = x + i, gy = y + j;
                if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
                float d = std::sqrt(i*i + j*j);
                if (d > radius) continue;
                dens[gy * sz + gx] += amount * (1 - d / radius);
            }
        }
    }

    void addVelocity(int x, int y, float vx, float vy, float radius) {
        int sz = N + 2;
        for (int j = -radius; j <= radius; j++) {
            for (int i = -radius; i <= radius; i++) {
                int gx = x + i, gy = y + j;
                if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
                float d = std::sqrt(i*i + j*j);
                if (d > radius) continue;
                float w = 1 - d / radius;
                u[gy * sz + gx] += vx * w;
                v[gy * sz + gx] += vy * w;
            }
        }
    }

    void step() {
        int sz = N + 2;

        // 1. Add external forces (gravity)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                v[j * sz + i] += 50.0f * dt;

        // 2. Advect velocity: write result to _prev, then swap
        advect(u_prev.data(), u.data(), u.data(), v.data(), dt);
        advect(v_prev.data(), v.data(), u.data(), v.data(), dt);
        std::swap(u, u_prev);
        std::swap(v, v_prev);

        // 3. Diffuse velocity
        diffuse(u_prev.data(), u.data(), visc, dt);
        diffuse(v_prev.data(), v.data(), visc, dt);
        std::swap(u, u_prev);
        std::swap(v, v_prev);

        // 4. Project (pressure solve + divergence-free correction)
        project(u.data(), v.data(), p.data(), div.data());

        // 5. Advect density
        advect(dens_prev.data(), dens.data(), u.data(), v.data(), dt);
        std::swap(dens, dens_prev);
    }

public:
    std::vector<float> u, v, u_prev, v_prev, dens, dens_prev, p, div;

    int IX(int i, int j) const { return j * (N + 2) + i; }

private:
    void bind(int& i, int& j) {
        if (i < 1) i = 1; if (i > N) i = N;
        if (j < 1) j = 1; if (j > N) j = N;
    }

    float lerp(float a, float b, float t) { return a + t * (b - a); }

    float sample(float* f, float x, float y) {
        int i = (int)x, j = (int)y;
        float fx = x - i, fy = y - j;
        i = std::max(1, std::min(N, i));
        j = std::max(1, std::min(N, j));
        int ni = std::min(i + 1, N);
        int nj = std::min(j + 1, N);
        int sz = N + 2;
        return lerp(lerp(f[IX(i,j)], f[IX(ni,j)], fx),
                    lerp(f[IX(i,nj)], f[IX(ni,nj)], fx), fy);
    }

    void advect(float* d, float* d0, float* u, float* v, float dt) {
        int sz = N + 2;
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                float x = i - dt * u[IX(i,j)];
                float y = j - dt * v[IX(i,j)];
                if (x < 0.5f) x = 0.5f; if (x > N + 0.5f) x = N + 0.5f;
                if (y < 0.5f) y = 0.5f; if (y > N + 0.5f) y = N + 0.5f;
                d[IX(i,j)] = sample(d0, x, y);
            }
        }
    }

    void diffuse(float* x, float* x0, float diff, float dt) {
        float a = dt * diff * N * N;
        int sz = N + 2;
        // Gauss-Seidel iterations
        for (int k = 0; k < 20; k++) {
            for (int j = 1; j <= N; j++) {
                for (int i = 1; i <= N; i++) {
                    x[IX(i,j)] = (x0[IX(i,j)] + a * (x[IX(i-1,j)] + x[IX(i+1,j)]
                                                    + x[IX(i,j-1)] + x[IX(i,j+1)]))
                                 / (1 + 4 * a);
                }
            }
            setBnd(x, 1);
        }
    }

    void project(float* u, float* v, float* p, float* div) {
        int sz = N + 2;
        // Compute divergence
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                div[IX(i,j)] = (u[IX(i+1,j)] - u[IX(i-1,j)]
                              + v[IX(i,j+1)] - v[IX(i,j-1)]) * (-0.5f / N);
                p[IX(i,j)] = 0;
            }
        }
        setBnd(div, 0);

        // Solve Poisson equation for pressure
        for (int k = 0; k < 30; k++) {
            for (int j = 1; j <= N; j++) {
                for (int i = 1; i <= N; i++) {
                    p[IX(i,j)] = (div[IX(i,j)]
                                + p[IX(i-1,j)] + p[IX(i+1,j)]
                                + p[IX(i,j-1)] + p[IX(i,j+1)]) * 0.25f;
                }
            }
            setBnd(p, 0);
        }

        // Subtract pressure gradient from velocity
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                u[IX(i,j)] -= 0.5f * N * (p[IX(i+1,j)] - p[IX(i-1,j)]);
                v[IX(i,j)] -= 0.5f * N * (p[IX(i,j+1)] - p[IX(i,j-1)]);
            }
        }
        setBnd(u, 1);
        setBnd(v, 2);
    }

    void setBnd(float* f, int b) {
        int sz = N + 2;
        // Horizontal walls
        for (int i = 1; i <= N; i++) {
            f[IX(i,0)]   = (b == 2) ? -f[IX(i,1)]   : f[IX(i,1)];
            f[IX(i,N+1)] = (b == 2) ? -f[IX(i,N)]   : f[IX(i,N)];
        }
        // Vertical walls
        for (int j = 1; j <= N; j++) {
            f[IX(0,j)]   = (b == 1) ? -f[IX(1,j)]   : f[IX(1,j)];
            f[IX(N+1,j)] = (b == 1) ? -f[IX(N,j)]   : f[IX(N,j)];
        }
        // Corners
        f[IX(0,0)]     = (f[IX(1,0)] + f[IX(0,1)]) * 0.5f;
        f[IX(N+1,0)]   = (f[IX(N,0)] + f[IX(N+1,1)]) * 0.5f;
        f[IX(0,N+1)]   = (f[IX(1,N+1)] + f[IX(0,N)]) * 0.5f;
        f[IX(N+1,N+1)] = (f[IX(N,N+1)] + f[IX(N+1,N)]) * 0.5f;
    }
};

// ─── Renderer ───────────────────────────────────────────────────────────────
class Renderer {
public:
    Renderer(SDL_Renderer* ren, int w, int h, int n)
        : m_ren(ren), m_w(w), m_h(h), m_n(n), m_tex(nullptr)
    {
        m_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, n, n);
    }

    ~Renderer() { if (m_tex) SDL_DestroyTexture(m_tex); }

    void present(FluidSolver& sol) {
        SDL_SetRenderDrawColor(m_ren, 6, 6, 18, 255);
        SDL_RenderClear(m_ren);

        int sz = sol.N + 2;
        std::vector<Uint32> pix(sol.N * sol.N);

        for (int j = 0; j < sol.N; j++) {
            for (int i = 0; i < sol.N; i++) {
                float d = sol.dens[(j + 1) * sz + (i + 1)];
                if (d < 0.01f) {
                    pix[j * sol.N + i] = 0xFF060612;
                    continue;
                }

                // Water color: transparent blue to opaque cyan
                float t = std::min(d / 0.6f, 1.0f);
                Uint8 r = (Uint8)(20 + t * 100);
                Uint8 g = (Uint8)(60 + t * 160);
                Uint8 b = (Uint8)(180 + t * 75);
                Uint8 a = (Uint8)(std::min(d / 0.15f, 1.0f) * 255);

                // Specular-style brightening on surface
                float surf = std::abs(d - 0.18f) < 0.05f ? 1.0f : 0.0f;
                r = (Uint8)std::min(255.0f, r + surf * 120);
                g = (Uint8)std::min(255.0f, g + surf * 120);
                b = (Uint8)std::min(255.0f, b + surf * 140);

                pix[j * sol.N + i] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }

        SDL_UpdateTexture(m_tex, nullptr, pix.data(), sol.N * 4);
        SDL_RenderCopy(m_ren, m_tex, nullptr, nullptr);
    }

private:
    SDL_Renderer* m_ren;
    SDL_Texture* m_tex;
    int m_w, m_h, m_n;
};

// ─── Main ───────────────────────────────────────────────────────────────────
int main() {
    std::srand(std::time(nullptr));

    const int WIN_W = 800, WIN_H = 600;
    const int N = 100; // 100×100 grid

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("Liquid — Fluid Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    FluidSolver sol(N, (float)WIN_W / N);
    sol.dt = 0.02f;

    // Place a blob of water
    sol.addDensity(N/2, N/2 - 5, 1.0f, 14);

    Renderer rend(ren, WIN_W, WIN_H, N);

    bool run = true;
    int mx = 0, my = 0;
    bool md = false;
    Uint32 lastTick = SDL_GetTicks();
    int frames = 0, fps = 0;
    Uint32 fpsT = SDL_GetTicks();

    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) run = false;
                if (e.key.keysym.sym == SDLK_r) {
                    // Reset: clear everything
                    std::fill(sol.dens.begin(), sol.dens.end(), 0.0f);
                    std::fill(sol.u.begin(), sol.u.end(), 0.0f);
                    std::fill(sol.v.begin(), sol.v.end(), 0.0f);
                    sol.addDensity(N/2, N/2 - 5, 1.0f, 14);
                }
                if (e.key.keysym.sym == SDLK_SPACE) {
                    sol.addDensity(N/2 - 8 + rand()%16, N/4, 0.8f, 8);
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
                { md = true; mx = e.button.x; my = e.button.y; }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                md = false;
            if (e.type == SDL_MOUSEMOTION && md) {
                int gx = mx * N / WIN_W;
                int gy = my * N / WIN_H;
                int ngx = e.motion.x * N / WIN_W;
                int ngy = e.motion.y * N / WIN_H;
                sol.addDensity(ngx, ngy, 0.5f, 3);
                sol.addVelocity(gx, gy,
                    (ngx - gx) * 80.0f,
                    (ngy - gy) * 80.0f, 4);
                mx = e.motion.x; my = e.motion.y;
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) * 0.001f;
        if (dt > 0.05f) dt = 0.05f;
        lastTick = now;

        sol.step();

        rend.present(sol);
        SDL_RenderPresent(ren);

        frames++;
        if (now - fpsT > 1000) {
            fps = frames;
            char buf[100];
            std::snprintf(buf, sizeof(buf),
                "Liquid  [%d FPS | %dx%d grid] Click+drag=stir  Space=splash  R=reset",
                fps, N, N);
            SDL_SetWindowTitle(win, buf);
            frames = 0; fpsT = now;
        }

        Uint32 ft = SDL_GetTicks() - now;
        if (ft < 16) SDL_Delay(16 - ft);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
