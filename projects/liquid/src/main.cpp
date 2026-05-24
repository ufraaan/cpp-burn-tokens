#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>

// ─── 2D Math ───────────────────────────────────────────────────────────────
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

// ─── Particle ───────────────────────────────────────────────────────────────
struct Particle {
    Vec2 pos;
    Vec2 vel;
    float dens;
    float pres;
    Particle() : pos(), vel(), dens(0), pres(0) {}
    Particle(float x, float y) : pos(x, y), vel(), dens(0), pres(0) {}
};

// ─── 2D SPH Kernels ─────────────────────────────────────────────────────────
// Normalized for 2D: ∫ W dA = 1
struct SPHKernels {
    float h;
    float h2;
    float poly6K;   // 4 / (pi * h^2)
    float spikyG;    // -30 / (pi * h^3)

    SPHKernels(float h_) : h(h_), h2(h_ * h_) {
        poly6K = 4.0f / (M_PI * h2);
        spikyG = -30.0f / (M_PI * h * h2);
    }

    float poly6(float r2) const {
        if (r2 < 0 || r2 > h2) return 0;
        float q = 1.0f - r2 / h2;
        return poly6K * q * q * q;
    }

    Vec2 spikyGrad(Vec2 dir, float r) const {
        if (r < 0.001f || r > h) return {0, 0};
        float q = 1.0f - r / h;
        return dir * (spikyG * q * q);
    }
};

// ─── SPH Solver ─────────────────────────────────────────────────────────────
struct SPHParams {
    float restDens = 1.0f;
    float gasConst = 1.0f;
    float viscosity = 0.25f;
    float gravity = 1.5f;
    float dt = 0.003f;
    float mass = 1.0f;
    float wallDamp = 0.3f;
    int subSteps = 2;
    float surfTens = 0.05f;
};

class SPHSolver {
public:
    SPHSolver(float ww, float wh, float h)
        : m_ww(ww), m_wh(wh), m_h(h), m_kernels(h)
    {
        m_gridW = (int)(ww / h) + 2;
        m_gridH = (int)(wh / h) + 2;
        m_gridNext.assign(m_gridW * m_gridH, -1);
    }

    void addParticle(float x, float y, float vx = 0, float vy = 0) {
        Particle p;
        p.pos = {x, y};
        p.vel = {vx, vy};
        m_parts.push_back(p);
        m_force.push_back({0, 0});
    }

    void addBlock(float x, float y, int n, float spacing) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                addParticle(x + i * spacing, y + j * spacing);
    }

    void reset() {
        m_parts.clear();
        m_force.clear();
        m_gridIdx.clear();
    }

    void update(const SPHParams& params) {
        float subDt = params.dt / params.subSteps;
        for (int s = 0; s < params.subSteps; s++) {
            buildGrid();
            computeDensity(params);
            computeForces(params);
            integrate(subDt, params);
            enforceBounds(params);
        }
    }

    const auto& parts() const { return m_parts; }
    float worldW() const { return m_ww; }
    float worldH() const { return m_wh; }
    float h() const { return m_h; }

private:
    void buildGrid() {
        // Reset head pointers
        std::fill(m_gridNext.begin(), m_gridNext.end(), -1);
        m_gridIdx.resize(m_parts.size());

        for (int i = 0; i < (int)m_parts.size(); i++) {
            int gx = (int)(m_parts[i].pos.x / m_h);
            int gy = (int)(m_parts[i].pos.y / m_h);
            if (gx < 0) gx = 0; if (gx >= m_gridW) gx = m_gridW - 1;
            if (gy < 0) gy = 0; if (gy >= m_gridH) gy = m_gridH - 1;
            int cell = gy * m_gridW + gx;
            m_gridIdx[i] = m_gridNext[cell];
            m_gridNext[cell] = i;
        }
    }

    template<typename Fn>
    void visitNeighbors(int i, const SPHParams& params, Fn&& fn) const {
        const Particle& pi = m_parts[i];
        int gx = (int)(pi.pos.x / m_h);
        int gy = (int)(pi.pos.y / m_h);
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int cx = gx + dx, cy = gy + dy;
                if (cx < 0 || cx >= m_gridW || cy < 0 || cy >= m_gridH) continue;
                int idx = m_gridNext[cy * m_gridW + cx];
                while (idx != -1) {
                    if (idx != i) fn(idx);
                    idx = m_gridIdx[idx];
                }
            }
        }
    }

    void computeDensity(const SPHParams& params) {
        for (auto& p : m_parts) p.dens = 0;

        for (int i = 0; i < (int)m_parts.size(); i++) {
            m_parts[i].dens += params.mass * m_kernels.poly6(0);
            visitNeighbors(i, params, [&](int j) {
                Vec2 d = m_parts[j].pos - m_parts[i].pos;
                float r2 = d.len2();
                m_parts[i].dens += params.mass * m_kernels.poly6(r2);
            });
        }

        for (auto& p : m_parts) {
            p.pres = params.gasConst * (p.dens - params.restDens);
            if (p.pres < 0) p.pres = 0;
        }
    }

    void computeForces(const SPHParams& params) {
        for (auto& f : m_force) f = {0, 0};

        for (int i = 0; i < (int)m_parts.size(); i++) {
            visitNeighbors(i, params, [&](int j) {
                Particle& pi = m_parts[i];
                Particle& pj = m_parts[j];
                Vec2 d = pj.pos - pi.pos;
                float r = d.len();
                if (r > m_h || r < 0.001f) return;
                Vec2 dir = d / r;

                float avgDens = (pi.dens + pj.dens) * 0.5f;
                float avgPres = (pi.pres + pj.pres) * 0.5f;

                Vec2 fp = m_kernels.spikyGrad(dir, r)
                        * (params.mass * avgPres / (avgDens * avgDens));

                Vec2 fv = (pj.vel - pi.vel)
                        * (params.mass * params.viscosity / avgDens)
                        * (m_kernels.poly6(r * r) * 0.5f); // viscosity kernel approx

                Vec2 ft = dir * (params.mass * params.surfTens
                        * m_kernels.poly6(r * r));

                Vec2 total = fp + fv + ft;
                m_force[i] += total;
                m_force[j] -= total;
            });
        }
    }

    void integrate(float dt, const SPHParams& params) {
        Vec2 grav(0, params.gravity);
        float invMass = 1.0f / params.mass;
        for (int i = 0; i < (int)m_parts.size(); i++) {
            Vec2 acc = m_force[i] * invMass + grav;
            m_parts[i].vel += acc * dt;
            m_parts[i].pos += m_parts[i].vel * dt;
        }
    }

    void enforceBounds(const SPHParams& params) {
        float r = 4.0f;
        for (auto& p : m_parts) {
            if (p.pos.x < r) { p.pos.x = r; p.vel.x = -p.vel.x * params.wallDamp; }
            if (p.pos.x > m_ww - r) { p.pos.x = m_ww - r; p.vel.x = -p.vel.x * params.wallDamp; }
            if (p.pos.y < r) { p.pos.y = r; p.vel.y = -p.vel.y * params.wallDamp; }
            if (p.pos.y > m_wh - r) { p.pos.y = m_wh - r; p.vel.y = -p.vel.y * params.wallDamp; }
        }
    }

    float m_ww, m_wh, m_h;
    SPHKernels m_kernels;
    std::vector<Particle> m_parts;
    std::vector<Vec2> m_force;
    std::vector<int> m_gridIdx;
    std::vector<int> m_gridNext;
    int m_gridW, m_gridH;

};

// ─── Density Field Renderer (Metaballs) ─────────────────────────────────────
class FluidRenderer {
public:
    FluidRenderer(SDL_Renderer* ren, int w, int h)
        : m_ren(ren), m_w(w), m_h(h)
    {
        m_fieldW = w / 4;
        m_fieldH = h / 4;
        m_field.assign(m_fieldW * m_fieldH, 0);
    }

    void present(const SPHSolver& solver, float mass) {
        SDL_SetRenderDrawColor(m_ren, 8, 8, 24, 255);
        SDL_RenderClear(m_ren);

        // Compute density field (same 2D poly6 as SPH solver, with mass)
        float h = solver.h();
        float h2 = h * h;
        float pk = 4.0f / (M_PI * h2) * mass;

        std::fill(m_field.begin(), m_field.end(), 0.0f);

        for (const auto& p : solver.parts()) {
            float cx = p.pos.x / 4.0f;
            float cy = p.pos.y / 4.0f;
            int rad = (int)(h / 4.0f) + 1;

            for (int dy = -rad; dy <= rad; dy++) {
                for (int dx = -rad; dx <= rad; dx++) {
                    int gx = (int)(cx) + dx;
                    int gy = (int)(cy) + dy;
                    if (gx < 0 || gx >= m_fieldW || gy < 0 || gy >= m_fieldH) continue;

                    float fx = gx * 4.0f + 2.0f;
                    float fy = gy * 4.0f + 2.0f;
                    float d2 = (p.pos.x - fx) * (p.pos.x - fx)
                             + (p.pos.y - fy) * (p.pos.y - fy);
                    if (d2 > h2) continue;

                    float q = 1.0f - d2 / h2;
                    m_field[gy * m_fieldW + gx] += pk * q * q * q;
                }
            }
        }

        // Render field directly with colored quads
        for (int y = 0; y < m_fieldH; y++) {
            for (int x = 0; x < m_fieldW; x++) {
                float d = m_field[y * m_fieldW + x];
                if (d < 0.1f) continue;

                Uint8 r, g, b;
                if (d < 0.3f) {
                    // Foam ring
                    float t = (d - 0.1f) / 0.2f;
                    float v = t * t * 200;
                    r = (Uint8)v; g = (Uint8)(v * 0.9f); b = (Uint8)v;
                } else {
                    // Water body — bright blue
                    float bright = std::min(1.0f, 0.5f + (d - 0.3f) / 0.5f * 0.5f);
                    r = (Uint8)(40 * bright);
                    g = (Uint8)(130 * bright);
                    b = (Uint8)(220 * bright);
                }

                SDL_SetRenderDrawColor(m_ren, r, g, b, 255);
                SDL_Rect rc = {x * 4, y * 4, 4, 4};
                SDL_RenderFillRect(m_ren, &rc);
            }
        }
    }

    float maxField() const {
        float mx = 0;
        for (float v : m_field) if (v > mx) mx = v;
        return mx;
    }

private:
    SDL_Renderer* m_ren;
    int m_w, m_h, m_fieldW, m_fieldH;
    std::vector<float> m_field;
};

// ─── Main ───────────────────────────────────────────────────────────────────
int main() {
    std::srand(std::time(nullptr));

    const int WIN_W = 780, WIN_H = 580;
    const float H = 22.0f;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* win = SDL_CreateWindow("Liquid — 2D Fluid Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SPHSolver solver(WIN_W, WIN_H, H);
    SPHParams params;
    params.mass = 80.0f;
    params.restDens = 1.0f;
    params.gasConst = 30000.0f;
    params.viscosity = 0.3f;
    params.gravity = 500.0f;
    params.dt = 0.003f;
    params.subSteps = 3;
    params.wallDamp = 0.3f;
    params.surfTens = 0.02f;

    // Initial water block
    solver.addBlock(180, 60, 20, 10);

    FluidRenderer renderer(ren, WIN_W, WIN_H);

    bool running = true;
    int mx = 0, my = 0;
    bool mdown = false;
    Uint32 lastTick = SDL_GetTicks();
    float spawnT = 0;
    int frames = 0, fps = 0;
    Uint32 fpsT = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_r:
                        solver.reset();
                        solver.addBlock(180, 60, 20, 10);
                        break;
                    case SDLK_SPACE:
                        solver.addBlock(200 + rand() % 200, 40, 8, 10);
                        break;
                    case SDLK_g:
                        params.gravity = (params.gravity == 0) ? 500.0f : 0.0f;
                        break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
                { mdown = true; mx = e.button.x; my = e.button.y; }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                mdown = false;
            if (e.type == SDL_MOUSEMOTION && mdown)
                { mx = e.motion.x; my = e.motion.y; }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) * 0.001f;
        if (dt > 0.05f) dt = 0.05f;
        lastTick = now;

        if (mdown) {
            spawnT += dt;
            while (spawnT > 0.008f) {
                solver.addParticle(
                    mx + (rand() % 20 - 10),
                    my + (rand() % 16 - 8),
                    (rand() % 80 - 40) * 0.2f,
                    -1.0f - (rand() % 50) * 0.05f);
                spawnT -= 0.008f;
            }
        } else spawnT = 0;

        solver.update(params);
        renderer.present(solver, params.mass);

        // HUD border
        SDL_SetRenderDrawColor(ren, 30, 35, 55, 40);
        SDL_Rect b = {0, 0, WIN_W - 1, WIN_H - 1};
        SDL_RenderDrawRect(ren, &b);

        SDL_RenderPresent(ren);

        frames++;
        if (now - fpsT > 1000) {
            fps = frames;
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "Liquid [%d FPS | %zu drops | max=%.2f] Click=spray  R=reset  Space=splash  G=gravity",
                fps, solver.parts().size(), renderer.maxField());
            SDL_SetWindowTitle(win, buf);
            frames = 0;
            fpsT = now;
        }

        Uint32 ft = SDL_GetTicks() - now;
        if (ft < 16) SDL_Delay(16 - ft);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
