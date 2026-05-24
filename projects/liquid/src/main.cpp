#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>
#include <algorithm>

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
        p.assign(sz * sz, 0.0f);
        div.assign(sz * sz, 0.0f);
    }

    int IX(int i, int j) const { return j * (N + 2) + i; }

    void addDensity(int x, int y, float amount, float radius) {
        for (int j = -(int)radius; j <= (int)radius; j++) {
            for (int i = -(int)radius; i <= (int)radius; i++) {
                int gx = x + i, gy = y + j;
                if (gx < 1 || gx > N || gy < 1 || gy > N) continue;
                float d = std::sqrt(i*i + j*j);
                if (d > radius) continue;
                dens[IX(gx, gy)] += amount * (1 - d / radius);
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

    void step(float gravity) {
        // 1. Gravity
        for (int j = 1; j <= N; j++)
            for (int i = 1; i <= N; i++)
                v[IX(i, j)] += gravity * dt;

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

        // 5. Advect density
        advect(dens_prev.data(), dens.data(), u.data(), v.data(), dt);
        std::swap(dens, dens_prev);
        setBnd(dens.data(), 0);
    }

    void reset() {
        std::fill(u.begin(), u.end(), 0.0f);
        std::fill(v.begin(), v.end(), 0.0f);
        std::fill(dens.begin(), dens.end(), 0.0f);
        std::fill(u_prev.begin(), u_prev.end(), 0.0f);
        std::fill(v_prev.begin(), v_prev.end(), 0.0f);
        std::fill(dens_prev.begin(), dens_prev.end(), 0.0f);
    }

    // Public data
    std::vector<float> u, v, u_prev, v_prev, dens, dens_prev, p, div;

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

    void advect(float* d, float* d0, float* u, float* v, float dt) {
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                float x = i - dt * u[IX(i, j)];
                float y = j - dt * v[IX(i, j)];
                if (x < 0.5f) x = 0.5f; if (x > N + 0.5f) x = N + 0.5f;
                if (y < 0.5f) y = 0.5f; if (y > N + 0.5f) y = N + 0.5f;
                d[IX(i, j)] = sample(d0, x, y);
            }
        }
    }

    void diffuse(float* x, float* x0, float diff, float dt) {
        float a = dt * diff * N * N;
        for (int k = 0; k < 20; k++) {
            for (int j = 1; j <= N; j++)
                for (int i = 1; i <= N; i++)
                    x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i-1, j)] + x[IX(i+1, j)]
                                                      + x[IX(i, j-1)] + x[IX(i, j+1)]))
                                 / (1 + 4 * a);
            setBnd(x, 1);
        }
    }

    void project(float* u, float* v, float* p, float* div) {
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                div[IX(i, j)] = (u[IX(i+1, j)] - u[IX(i-1, j)]
                               + v[IX(i, j+1)] - v[IX(i, j-1)]) * (-0.5f / N);
                p[IX(i, j)] = 0;
            }
        }
        setBnd(div, 0);
        for (int k = 0; k < 30; k++) {
            for (int j = 1; j <= N; j++)
                for (int i = 1; i <= N; i++)
                    p[IX(i, j)] = (div[IX(i, j)]
                                 + p[IX(i-1, j)] + p[IX(i+1, j)]
                                 + p[IX(i, j-1)] + p[IX(i, j+1)]) * 0.25f;
            setBnd(p, 0);
        }
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
        // Render density field into texture
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

                float t = std::min(d / 0.5f, 1.0f);
                float depth = std::min(d / 0.3f, 1.0f);

                Uint8 r = (Uint8)(15 + t * 110);
                Uint8 g = (Uint8)(45 + t * 180);
                Uint8 b = (Uint8)(130 + t * 125);
                Uint8 a = (Uint8)(std::min(d / 0.12f, 1.0f) * 255);

                // Specular shimmer from density gradient (neighbors)
                float dx = sol.dens[sol.IX(std::min(gi+1,sol.N), gj)]
                         - sol.dens[sol.IX(std::max(gi-1,1), gj)];
                float dy = sol.dens[sol.IX(gi, std::min(gj+1,sol.N))]
                         - sol.dens[sol.IX(gi, std::max(gj-1,1))];
                float spec = std::abs(dx) + std::abs(dy);
                float sh = std::min(spec * 3.0f, 1.0f) * 0.35f;
                r = (Uint8)std::min(255.0f, r + sh * 180);
                g = (Uint8)std::min(255.0f, g + sh * 160);
                b = (Uint8)std::min(255.0f, b + sh * 120);

                m_pix[j * m_texW + i] = (a << 24) | (r << 16) | (g << 8) | b;
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

    const int WIN_W = 960, WIN_H = 740;
    const int N = 150;
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
    float gravity = 6.0f;

    // Initial water blob
    sol.addDensity(N/2, N/3, 1.0f, 14);

    // Renderer at 2x grid resolution for smoother display
    Renderer rend(ren, N, N * 2, N * 2);

    HUD fpsHUD(ren, font);
    HUD infoHUD(ren, font);
    HUD ctrlHUD(ren, font);
    fpsHUD.setText("FPS: --  Particles: 0");
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
                        sol.addDensity(N/2, N/3, 1.0f, 14);
                        break;
                    case SDLK_SPACE:
                        sol.addDensity(N/2 - 12 + rand()%24, N/6, 1.0f, 10);
                        break;
                    case SDLK_g:
                        gravity = (gravity == 0) ? 6.0f : 0.0f;
                        break;
                    default: break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
                { md = true; mx = e.button.x; my = e.button.y; }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                md = false;
            if (e.type == SDL_MOUSEMOTION && md) {
                int gx = (mx - PAD) * N / CONT_W;
                int gy = (my - PAD) * N / CONT_H;
                int ngx = (e.motion.x - PAD) * N / CONT_W;
                int ngy = (e.motion.y - PAD) * N / CONT_H;
                if (gx >= 1 && gx <= N && gy >= 1 && gy <= N &&
                    ngx >= 1 && ngx <= N && ngy >= 1 && ngy <= N) {
                    sol.addDensity(ngx, ngy, 0.4f, 4);
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
        int totalDrops = 0;
        for (float d : sol.dens) if (d > 0.05f) totalDrops++;

        char buf[100];
        std::snprintf(buf, sizeof(buf), "FPS: %d  Fluid density cells: %d", fps, totalDrops);
        fpsHUD.setText(buf);
        fpsHUD.draw(PAD, 6);

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
