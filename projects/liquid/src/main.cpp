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

struct Particle {
    Vec2 pos, vel;
    float dens, pres;
    Particle() : pos(), vel(), dens(0), pres(0) {}
    Particle(float x, float y) : pos(x, y), vel(), dens(0), pres(0) {}
};

// ─── 2D SPH ─────────────────────────────────────────────────────────────────
struct SPH {
    float h, h2;
    float poly6K;  // 4/(pi*h^2)
    float spikyG;  // -30/(pi*h^3)

    SPH(float h_) : h(h_), h2(h_*h_) {
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

struct Params {
    float mass = 80;
    float restDens = 1;
    float gasConst = 30000;
    float viscosity = 0.3f;
    float gravity = 500;
    float dt = 0.005f;
    float wallDamp = 0.3f;
    float surfTens = 0.02f;
};

class Solver {
public:
    Solver(float ww, float wh, float h) : m_ww(ww), m_wh(wh), m_h(h), m_sph(h) {
        m_gw = (int)(ww / h) + 2;
        m_gh = (int)(wh / h) + 2;
        m_next.assign(m_gw * m_gh, -1);
    }

    void add(float x, float y, float vx = 0, float vy = 0) {
        m_p.push_back({x, y});
        m_p.back().vel = {vx, vy};
        m_f.push_back({0, 0});
    }

    void addBlock(float x, float y, int n, float sp) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                add(x + i * sp, y + j * sp);
    }

    void reset() { m_p.clear(); m_f.clear(); }

    void step(const Params& pr) {
        // --- Build grid ---
        std::fill(m_next.begin(), m_next.end(), -1);
        m_link.resize(m_p.size());
        for (int i = 0; i < (int)m_p.size(); i++) {
            int gx = (int)(m_p[i].pos.x / m_h);
            int gy = (int)(m_p[i].pos.y / m_h);
            if (gx < 0) gx = 0; if (gx >= m_gw) gx = m_gw - 1;
            if (gy < 0) gy = 0; if (gy >= m_gh) gy = m_gh - 1;
            int c = gy * m_gw + gx;
            m_link[i] = m_next[c];
            m_next[c] = i;
        }

        // --- Density ---
        auto visit = [&](int i, auto&& fn) {
            int gx = (int)(m_p[i].pos.x / m_h);
            int gy = (int)(m_p[i].pos.y / m_h);
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int cx = gx + dx, cy = gy + dy;
                    if (cx < 0 || cx >= m_gw || cy < 0 || cy >= m_gh) continue;
                    int idx = m_next[cy * m_gw + cx];
                    while (idx != -1) {
                        if (idx != i) fn(idx);
                        idx = m_link[idx];
                    }
                }
            }
        };

        for (auto& p : m_p) p.dens = 0;
        for (int i = 0; i < (int)m_p.size(); i++) {
            m_p[i].dens += pr.mass * m_sph.poly6(0);
            visit(i, [&](int j) {
                float r2 = (m_p[j].pos - m_p[i].pos).len2();
                m_p[i].dens += pr.mass * m_sph.poly6(r2);
            });
        }
        for (auto& p : m_p) {
            p.pres = pr.gasConst * (p.dens - pr.restDens);
            if (p.pres < 0) p.pres = 0;
        }

        // --- Forces ---
        for (auto& f : m_f) f = {0, 0};
        for (int i = 0; i < (int)m_p.size(); i++) {
            visit(i, [&](int j) {
                auto& pi = m_p[i], & pj = m_p[j];
                Vec2 d = pj.pos - pi.pos;
                float r = d.len();
                if (r > m_h || r < 0.001f) return;
                Vec2 dir = d / r;

                float avgD = (pi.dens + pj.dens) * 0.5f;
                float avgP = (pi.pres + pj.pres) * 0.5f;

                Vec2 fp = m_sph.spikyGrad(dir, r) * (pr.mass * avgP / (avgD * avgD));
                Vec2 fv = (pj.vel - pi.vel) * (pr.mass * pr.viscosity / avgD) * (m_sph.poly6(r*r) * 0.5f);
                Vec2 ft = dir * (pr.mass * pr.surfTens * m_sph.poly6(r*r));

                Vec2 total = fp + fv + ft;
                m_f[i] += total;
                m_f[j] -= total;
            });
        }

        // --- Integrate ---
        float invM = 1.0f / pr.mass;
        Vec2 grav(0, pr.gravity);
        for (int i = 0; i < (int)m_p.size(); i++) {
            Vec2 acc = m_f[i] * invM + grav;
            m_p[i].vel += acc * pr.dt;
            m_p[i].pos += m_p[i].vel * pr.dt;
        }

        // --- Bounds ---
        float r = 4;
        for (auto& p : m_p) {
            if (p.pos.x < r) { p.pos.x = r; p.vel.x = -p.vel.x * pr.wallDamp; }
            if (p.pos.x > m_ww - r) { p.pos.x = m_ww - r; p.vel.x = -p.vel.x * pr.wallDamp; }
            if (p.pos.y < r) { p.pos.y = r; p.vel.y = -p.vel.y * pr.wallDamp; }
            if (p.pos.y > m_wh - r) { p.pos.y = m_wh - r; p.vel.y = -p.vel.y * pr.wallDamp; }
        }
    }

    const auto& parts() const { return m_p; }
    float worldW() const { return m_ww; }
    float worldH() const { return m_wh; }
    float h() const { return m_h; }

private:
    float m_ww, m_wh, m_h;
    SPH m_sph;
    std::vector<Particle> m_p;
    std::vector<Vec2> m_f;
    std::vector<int> m_link, m_next;
    int m_gw, m_gh;
};

// ─── Particle Sprite Renderer ───────────────────────────────────────────────
class Renderer {
public:
    Renderer(SDL_Renderer* ren) : m_ren(ren), m_sz(32) {
        m_sprite = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STATIC, m_sz, m_sz);
        SDL_SetTextureBlendMode(m_sprite, SDL_BLENDMODE_BLEND);

        std::vector<Uint32> pix(m_sz * m_sz, 0x00000000);
        int half = m_sz / 2;
        for (int y = 0; y < m_sz; y++) {
            for (int x = 0; x < m_sz; x++) {
                float dx = x - half, dy = y - half;
                float r = std::sqrt(dx*dx + dy*dy) / half;
                if (r >= 1) continue;
                float a = (1.0f - r*r) * 200;
                Uint32 ai = (Uint32)a;
                pix[y * m_sz + x] = (ai << 24) | (ai << 16) | (ai << 8) | ai;
            }
        }
        SDL_UpdateTexture(m_sprite, nullptr, pix.data(), m_sz * 4);
    }

    ~Renderer() { if (m_sprite) SDL_DestroyTexture(m_sprite); }

    void present(const Solver& sol) {
        SDL_SetRenderDrawColor(m_ren, 6, 6, 18, 255);
        SDL_RenderClear(m_ren);

        int half = m_sz / 2;
        for (const auto& p : sol.parts()) {
            float spd = p.vel.len();
            float t = std::min(spd / 300.0f, 1.0f);

            // Deep blue base → bright cyan as speed increases
            Uint8 r = (Uint8)(15 + t * 110);
            Uint8 g = (Uint8)(50 + t * 180);
            Uint8 b = (Uint8)(140 + t * 115);

            SDL_SetTextureColorMod(m_sprite, r, g, b);
            SDL_SetTextureAlphaMod(m_sprite, 190);

            SDL_Rect dst = {(int)p.pos.x - half, (int)p.pos.y - half, m_sz, m_sz};
            SDL_RenderCopy(m_ren, m_sprite, nullptr, &dst);
        }
    }

private:
    SDL_Renderer* m_ren;
    SDL_Texture* m_sprite;
    int m_sz;
};

// ─── Main ───────────────────────────────────────────────────────────────────
int main() {
    std::srand(std::time(nullptr));

    const int W = 780, H = 580;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("Liquid — 2D Fluid Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    Solver sol(W, H, 22);
    Params pr;
    sol.addBlock(180, 30, 20, 10); // higher up so it falls longer

    Renderer rend(ren);

    bool run = true;
    int mx = 0, my = 0;
    bool md = false;
    Uint32 lastTick = SDL_GetTicks();
    float spawnT = 0;
    int frames = 0, fps = 0;
    Uint32 fpsT = SDL_GetTicks();

    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: run = false; break;
                    case SDLK_r: sol.reset(); sol.addBlock(180, 30, 20, 10); break;
                    case SDLK_SPACE: sol.addBlock(200 + rand()%200, 40, 8, 10); break;
                    case SDLK_g: pr.gravity = (pr.gravity == 0) ? 500.0f : 0.0f; break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
                { md = true; mx = e.button.x; my = e.button.y; }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) md = false;
            if (e.type == SDL_MOUSEMOTION && md) { mx = e.motion.x; my = e.motion.y; }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) * 0.001f;
        if (dt > 0.05f) dt = 0.05f;
        lastTick = now;

        if (md) {
            spawnT += dt;
            while (spawnT > 0.008f) {
                sol.add(mx + (rand()%20 - 10), my + (rand()%16 - 8),
                    (rand()%80 - 40) * 0.2f, -1 - (rand()%50) * 0.05f);
                spawnT -= 0.008f;
            }
        } else spawnT = 0;

        sol.step(pr);
        rend.present(sol);
        SDL_RenderPresent(ren);

        frames++;
        if (now - fpsT > 1000) {
            fps = frames;
            char buf[100];
            std::snprintf(buf, sizeof(buf),
                "Liquid [%d FPS | %zu drops] Click=spray  R=reset  Space=splash  G=gravity",
                fps, sol.parts().size());
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
