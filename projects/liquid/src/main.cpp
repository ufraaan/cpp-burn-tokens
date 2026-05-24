#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>
#include <algorithm>
#include <omp.h>

// ─── 2D Navier-Stokes solver ────────────────────────────────────────────────
// MAC grid: velocities & density at cell centers. Ghost cells 0 and N+1.

class FluidSolver {
public:
    int N;
    float dt;
    float visc;

    FluidSolver(int n)
        : N(n), dt(0.02f), visc(0.0005f)
    {
        int sz = N + 2;
        u.assign(sz * sz, 0.0f);
        v.assign(sz * sz, 0.0f);
        u_prev.assign(sz * sz, 0.0f);
        v_prev.assign(sz * sz, 0.0f);
        dens.assign(sz * sz, 0.0f);
        dens_prev.assign(sz * sz, 0.0f);
        hue.assign(sz * sz, 0.5f);
        hue_prev.assign(sz * sz, 0.5f);
        p.assign(sz * sz, 0.0f);
        div.assign(sz * sz, 0.0f);
        wk.assign(sz * sz, 0.0f);
    }

    int IX(int i, int j) const { return j * (N + 2) + i; }

    void addDensityLine(int x1, int y1, int x2, int y2, float amount, float radius, float h = 0.5f) {
        int steps = std::max(std::abs(x2 - x1), std::abs(y2 - y1));
        if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            float t = (float)s / steps;
            int cx = x1 + (int)((x2 - x1) * t);
            int cy = y1 + (int)((y2 - y1) * t);
            addDensity(cx, cy, amount, radius, h);
        }
    }

    void addDensity(int x, int y, float amount, float radius, float h = 0.5f) {
        for (int j = -(int)radius; j <= (int)radius; j++) {
            for (int i = -(int)radius; i <= (int)radius; i++) {
                int gx = x + i, gy = y + j;
                if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
                float d = std::sqrt(i*i + j*j);
                if (d > radius) continue;
                float w = amount * (1 - d / radius);
                float old_dens = dens[IX(gx, gy)];
                float old_hue = hue[IX(gx, gy)];
                dens[IX(gx, gy)] += w;
                if (dens[IX(gx, gy)] > 0)
                    hue[IX(gx, gy)] = (old_hue * old_dens + h * w) / dens[IX(gx, gy)];
            }
        }
    }

    void addVelocity(int x, int y, float vx, float vy, float radius) {
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

    void addSplash(int cx, int cy, float intensity) {
        int r = 15;
        addDensity(cx, cy, intensity, r, 1.0f);
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
                    dens[IX(gx, gy)] += 0.4f * (1.0f - dist / r);
                    float od = dens[IX(gx, gy)] - 0.4f * (1.0f - dist / r);
                    if (dens[IX(gx, gy)] > 0)
                        hue[IX(gx, gy)] = (hue[IX(gx, gy)] * std::max(0.0f, od) + 1.0f * 0.4f * (1.0f - dist / r)) / dens[IX(gx, gy)];
                }
            }
        }
    }

    void addVorticityConfinement(float eps) {
        // Compute vorticity ω = ∂v/∂x - ∂u/∂y
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                wk[IX(i, j)] = (v[IX(i+1, j)] - v[IX(i-1, j)]
                              - u[IX(i, j+1)] + u[IX(i, j-1)]) * 0.5f * N;
        // Confinement force: f = eps * (∇|ω| × ω)  (perpendicular to grad)
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

    void step(float gravity, float vort_eps = 8.0f) {
        // 1. Vorticity confinement (re-injects swirling eddies)
        addVorticityConfinement(vort_eps);

        // 2. Gravity — only where water exists
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                v[IX(i, j)] += gravity * dt * std::min(dens[IX(i, j)], 1.0f);

        // 2. Advect velocity
        advect(u_prev.data(), u.data(), u.data(), v.data(), dt);
        advect(v_prev.data(), v.data(), u.data(), v.data(), dt);
        std::swap(u, u_prev);
        std::swap(v, v_prev);

        // 3. Diffuse velocity
        diffuse(u_prev.data(), u.data(), visc, dt);
        diffuse(v_prev.data(), v.data(), visc, dt);
        std::swap(u, u_prev);
        std::swap(v, v_prev);

        // 4. Project
        project(u.data(), v.data(), p.data(), div.data());

        // 5. Advect density with reflective walls
        advect(dens_prev.data(), dens.data(), u.data(), v.data(), dt, true);
        std::swap(dens, dens_prev);
        setBnd(dens.data(), 0);

        // 5b. Advect hue with reflective walls
        advect(hue_prev.data(), hue.data(), u.data(), v.data(), dt, true);
        std::swap(hue, hue_prev);
        setBnd(hue.data(), 0);

        // 6. Cap density to prevent runaway accumulation at walls
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                dens[IX(i, j)] = std::min(dens[IX(i, j)], 1.0f);
    }

    void reset() {
        std::fill(u.begin(), u.end(), 0.0f);
        std::fill(v.begin(), v.end(), 0.0f);
        std::fill(dens.begin(), dens.end(), 0.0f);
        std::fill(hue.begin(), hue.end(), 0.5f);
        std::fill(u_prev.begin(), u_prev.end(), 0.0f);
        std::fill(v_prev.begin(), v_prev.end(), 0.0f);
        std::fill(dens_prev.begin(), dens_prev.end(), 0.0f);
        std::fill(hue_prev.begin(), hue_prev.end(), 0.5f);
    }

    // Public data
    std::vector<float> u, v, u_prev, v_prev, dens, dens_prev, hue, hue_prev, p, div, wk;

private:
    void setBnd(float* f, int b) {
        // Horizontal walls
        for (int i = 1; i <= N; i++) {
            f[IX(i, 0)]   = (b == 2) ? -f[IX(i, 1)]   : f[IX(i, 1)];
            f[IX(i, N+1)] = (b == 2) ? -f[IX(i, N)]   : f[IX(i, N)];
        }
        // Vertical walls
        for (int j = 1; j <= N; j++) {
            f[IX(0, j)]   = (b == 1) ? -f[IX(1, j)]   : f[IX(1, j)];
            f[IX(N+1, j)] = (b == 1) ? -f[IX(N, j)]   : f[IX(N, j)];
        }
        // Corners
        f[IX(0, 0)]     = (f[IX(1, 0)] + f[IX(0, 1)]) * 0.5f;
        f[IX(N+1, 0)]   = (f[IX(N, 0)] + f[IX(N+1, 1)]) * 0.5f;
        f[IX(0, N+1)]   = (f[IX(1, N+1)] + f[IX(0, N)]) * 0.5f;
        f[IX(N+1, N+1)] = (f[IX(N, N+1)] + f[IX(N+1, N)]) * 0.5f;
    }

    float lerp(float a, float b, float t) { return a + t * (b - a); }

    float sample(float* f, float x, float y) {
        int i = (int)x, j = (int)y;
        float fx = x - i, fy = y - j;
        i = std::max(1, std::min(N, i));
        j = std::max(1, std::min(N, j));
        int ni = std::min(i + 1, N);
        int nj = std::min(j + 1, N);
        return lerp(lerp(f[IX(i, j)], f[IX(ni, j)], fx),
                    lerp(f[IX(i, nj)], f[IX(ni, nj)], fx), fy);
    }

    void advect(float* d, float* d0, float* u, float* v, float dt, bool reflect = false) {
        #pragma omp parallel for collapse(2)
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                float x = i - dt * u[IX(i, j)];
                float y = j - dt * v[IX(i, j)];
                if (reflect) {
                    // Iterative reflective walls: keep bouncing until inside
                    for (int it = 0; it < 8; it++) {
                        bool ok = true;
                        if (x < 0.5f) { x = 1.0f - x; ok = false; }
                        else if (x > N + 0.5f) { x = 2.0f * (N + 0.5f) - x; ok = false; }
                        if (y < 0.5f) { y = 1.0f - y; ok = false; }
                        else if (y > N + 0.5f) { y = 2.0f * (N + 0.5f) - y; ok = false; }
                        if (ok) break;
                    }
                    // Final clamp -> ghost cells will be zeroed anyway
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

    void diffuse(float* x, float* x0, float diff, float dt) {
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

    void project(float* u, float* v, float* p, float* div) {
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
};

// ─── Renderer ───────────────────────────────────────────────────────────────
class Renderer {
public:
    Renderer(SDL_Renderer* ren, int n, int texW, int texH)
        : m_ren(ren), m_n(n), m_texW(texW), m_texH(texH)
    {
        m_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, texW, texH);
        m_pix.assign(texW * texH, 0);
    }

    ~Renderer() { if (m_tex) SDL_DestroyTexture(m_tex); }

    void present(FluidSolver& sol, int offX, int offY, int drawW, int drawH) {
        // Render density field into texture with 3D height-field lighting
        int sz = sol.N + 2;
        float scaleX = (float)m_texW / sol.N;
        float scaleY = (float)m_texH / sol.N;

        for (int j = 0; j < m_texH; j++) {
            for (int i = 0; i < m_texW; i++) {
                int gi = (int)(i / scaleX) + 1;
                int gj = (int)(j / scaleY) + 1;
                if (gi > sol.N) gi = sol.N;
                if (gj > sol.N) gj = sol.N;
                float d = sol.dens[sol.IX(gi, gj)];

                if (d < 0.008f) {
                    m_pix[j * m_texW + i] = 0xFF08081A;
                    continue;
                }

                float h = std::min(d / 0.4f, 1.0f);

                // Normal from density gradient (height map)
                int gi1 = std::max(gi - 1, 1);
                int gi2 = std::min(gi + 1, sol.N);
                int gj1 = std::max(gj - 1, 1);
                int gj2 = std::min(gj + 1, sol.N);
                float dl = sol.dens[sol.IX(gi1, gj)];
                float dr = sol.dens[sol.IX(gi2, gj)];
                float du = sol.dens[sol.IX(gi, gj1)];
                float dd = sol.dens[sol.IX(gi, gj2)];

                float nx = (dl - dr) * 0.8f;
                float ny = (du - dd) * 0.8f;
                float nz = 1.0f;
                float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (nl > 0) { nx /= nl; ny /= nl; nz /= nl; }

                // Light from upper-right
                float lx = 0.3f, ly = -0.4f, lz = 0.8f;
                float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
                lx /= ll; ly /= ll; lz /= ll;

                float diff = std::max(0.0f, nx * lx + ny * ly + nz * lz);

                // Blinn-Phong specular
                float hx = lx, hy = ly, hz = lz + 1.0f;
                float hlen = std::sqrt(hx * hx + hy * hy + hz * hz);
                hx /= hlen; hy /= hlen; hz /= hlen;
                float spec = std::pow(std::max(0.0f, nx * hx + ny * hy + nz * hz), 16.0f);

                float ambient = 0.25f;
                float light = std::min(ambient + diff * 0.75f + spec * 0.5f, 1.0f);

                // Base color from hue: 0=red, 0.5=purple, 1=blue
                float hue_val = sol.hue[sol.IX(gi, gj)];
                float r = (180.0f * (1.0f - hue_val) + 30.0f * hue_val) / 255.0f;
                float g = (40.0f * (1.0f - hue_val) + 60.0f * hue_val) / 255.0f;
                float b = (50.0f * (1.0f - hue_val) + 200.0f * hue_val) / 255.0f;

                Uint8 pr = (Uint8)std::min(255.0f, r * light * 255.0f + spec * 200.0f);
                Uint8 pg = (Uint8)std::min(255.0f, g * light * 255.0f + spec * 200.0f);
                Uint8 pb = (Uint8)std::min(255.0f, b * light * 255.0f + spec * 200.0f);
                Uint8 pa = (Uint8)(std::min(d / 0.12f, 1.0f) * 255);

                m_pix[j * m_texW + i] = (pa << 24) | (pr << 16) | (pg << 8) | pb;
            }
        }

        SDL_UpdateTexture(m_tex, nullptr, m_pix.data(), m_texW * 4);

        // Draw background (container interior)
        SDL_SetRenderDrawColor(m_ren, 8, 8, 26, 255);
        SDL_RenderClear(m_ren);

        // Draw the fluid texture within the container area
        SDL_Rect dst = {offX, offY, drawW, drawH};
        SDL_RenderCopy(m_ren, m_tex, nullptr, &dst);

        // Container border
        SDL_SetRenderDrawColor(m_ren, 35, 40, 65, 255);
        SDL_Rect border = {offX - 1, offY - 1, drawW + 2, drawH + 2};
        SDL_RenderDrawRect(m_ren, &border);
    }

private:
    SDL_Renderer* m_ren;
    SDL_Texture* m_tex;
    std::vector<Uint32> m_pix;
    int m_n, m_texW, m_texH;
};

// ─── Text HUD ───────────────────────────────────────────────────────────────
class HUD {
public:
    HUD(SDL_Renderer* ren, TTF_Font* font)
        : m_ren(ren), m_font(font), m_tex(nullptr), m_w(0), m_h(0) {}

    void setText(const char* text) {
        if (m_tex) { SDL_DestroyTexture(m_tex); m_tex = nullptr; }
        SDL_Color col = {170, 175, 200, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(m_font, text, col);
        if (!surf) return;
        m_w = surf->w; m_h = surf->h;
        m_tex = SDL_CreateTextureFromSurface(m_ren, surf);
        SDL_FreeSurface(surf);
    }

    void draw(int x, int y) {
        if (!m_tex) return;
        SDL_Rect dst = {x, y, m_w, m_h};
        SDL_RenderCopy(m_ren, m_tex, nullptr, &dst);
    }

    int width() const { return m_w; }
    int height() const { return m_h; }

    ~HUD() { if (m_tex) SDL_DestroyTexture(m_tex); }

private:
    SDL_Renderer* m_ren;
    TTF_Font* m_font;
    SDL_Texture* m_tex;
    int m_w, m_h;
};

// ─── Main ───────────────────────────────────────────────────────────────────
int main() {
    std::srand(std::time(nullptr));

    const int WIN_W = 960, WIN_H = 960;
    const int N = 600;
    const int PAD = 30;              // container padding from window edges
    const int CONT_W = WIN_W - PAD * 2;
    const int CONT_H = WIN_H - PAD * 2;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* win = SDL_CreateWindow("Liquid — Fluid Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 13);
    if (!font) font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 13);
    if (!font) font = TTF_OpenFont("/System/Fonts/PingFang.ttc", 13);

    FluidSolver sol(N);
    float gravity = 2.0f;

    // Initial water blob
    sol.addDensity(N/2, N/4, 1.0f, 14);

    // Renderer at 2x grid resolution for smoother display
    Renderer rend(ren, N, N * 2, N * 2);

    HUD fpsHUD(ren, font);
    HUD infoHUD(ren, font);
    HUD ctrlHUD(ren, font);
    HUD gravHUD(ren, font);
    fpsHUD.setText("FPS: --");
    infoHUD.setText("Liquid — Eulerian Navier-Stokes Fluid");
    ctrlHUD.setText("Click+Drag = Stir    Space = Splash    R = Reset    G = Gravity");

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
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: run = false; break;
                    case SDLK_r:
                        sol.reset();
                        sol.addDensity(N/2, N/4, 1.0f, 14);
                        break;
                    case SDLK_SPACE:
                        sol.addSplash(N/2 - 12 + rand()%24, N/6, 1.0f);
                        break;
                    case SDLK_g:
                        gravity = (gravity == 0) ? 2.0f : 0.0f;
                        break;
                    default: break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_CaptureMouse(SDL_TRUE);  // capture so BUTTONUP always fires
                md = true; mx = e.button.x; my = e.button.y;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                SDL_CaptureMouse(SDL_FALSE);
                md = false;
            }
            if (e.type == SDL_MOUSEMOTION && md) {
                int gx = (mx - PAD) * N / CONT_W;
                int gy = (my - PAD) * N / CONT_H;
                int ngx = (e.motion.x - PAD) * N / CONT_W;
                int ngy = (e.motion.y - PAD) * N / CONT_H;
                if (gx >= 1 && gx <= N && gy >= 1 && gy <= N &&
                    ngx >= 1 && ngx <= N && ngy >= 1 && ngy <= N) {
                    sol.addDensityLine(gx, gy, ngx, ngy, 0.4f, 4, 0.0f);
                    sol.addVelocity(gx, gy,
                        (ngx - gx) * 60.0f, (ngy - gy) * 60.0f, 5);
                }
                mx = e.motion.x; my = e.motion.y;
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) * 0.001f;
        if (dt > 0.05f) dt = 0.05f;
        lastTick = now;

        sol.step(gravity);

        // Render fluid
        rend.present(sol, PAD, PAD, CONT_W, CONT_H);

        // Draw HUD text
        char buf[100];
        std::snprintf(buf, sizeof(buf), "FPS: %d", fps);
        fpsHUD.setText(buf);
        fpsHUD.draw(PAD, 6);

        gravHUD.setText(gravity > 0 ? "Gravity: ON" : "Gravity: OFF");
        gravHUD.draw(WIN_W - PAD - 100, 6);

        ctrlHUD.draw(PAD, WIN_H - 28);

        int iw = infoHUD.width();
        infoHUD.draw((WIN_W - iw) / 2, 6);

        SDL_RenderPresent(ren);

        frames++;
        if (now - fpsT > 1000) {
            fps = frames;
            char buf[100];
            std::snprintf(buf, sizeof(buf),
                "Liquid [%d FPS | %dx%d grid]", fps, N, N);
            SDL_SetWindowTitle(win, buf);
            frames = 0; fpsT = now;
        }

        Uint32 ft = SDL_GetTicks() - now;
        if (ft < 16) SDL_Delay(16 - ft);
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
