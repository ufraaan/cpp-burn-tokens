#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <chrono>
#include "gpu_tracer.hpp"

static const GpuScene SCENE = {{
    {{0, -1001, 0}, 1000, {0.5f, 0.5f, 0.5f}, 0.08f, 0, 0.3f},
    {{-1.3f, 0.25f, 2.2f}, 0.5f, {1, 0.12f, 0.12f}, 0, 0, 1},
    {{1.4f, 0.15f, 1.8f}, 0.65f, {0.1f, 0.15f, 1}, 0.4f, 0, 0.15f},
    {{0.1f, 0.25f, 3.5f}, 0.5f, {0.8f, 0.9f, 0.85f}, 0, 1.5f, 0},
    {{-0.8f, 0.8f, 2.5f}, 0.35f, {0.85f, 0.85f, 0.9f}, 0.85f, 0, 0.05f},
    {{-0.2f, -0.3f, 1.5f}, 0.3f, {1, 0.7f, 0.1f}, 0.6f, 0, 0.3f},
    {{-1.5f, 0.6f, 3.5f}, 0.2f, {0.95f, 0.95f, 1}, 0.7f, 0, 0.1f},
    {{1.6f, -0.1f, 3.5f}, 0.35f, {0.6f, 0.2f, 0.8f}, 0, 0, 1},
}, {0.8f, 1, 0.6f}, {0, 2.6f, 0.5f}, {0.7f, 0, 0}, {0, 0, 0.7f}, {20, 18, 15}};

static SDL_Texture* make_text(SDL_Renderer* ren, TTF_Font* font, const char* text, SDL_Color color, int& w, int& h) {
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    w = surf->w; h = surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* win = SDL_CreateWindow("GPU Ray Tracer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    int W, H;
    SDL_GetRendererOutputSize(ren, &W, &H);

    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, W, H);

    printf("Rendering %dx%d | 4x4 AA | 8 bounces | 24x area light | DOF | glass | glossy\n", W, H);
    fflush(stdout);

    auto t0 = std::chrono::steady_clock::now();
    GpuTracer tracer(W, H, SCENE);
    tracer.render();
    auto t1 = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();

    SDL_UpdateTexture(tex, nullptr, tracer.pixels(), W * 4);
    printf("Done in %.0f ms.\n", ms);

    // ── HUD overlay ────────────────────────────────────────────────────
    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 20);
    if (!font) font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Courier New.ttf", 20);

    SDL_Color white = {220, 220, 220, 220};
    SDL_Color dim   = {160, 160, 160, 200};

    char line1[128], line2[128], line3[128], line4[128];
    snprintf(line1, sizeof line1, "%dx%d  |  4x4 AA  |  %d bounces", W, H, 8);
    snprintf(line2, sizeof line2, "area light 24x  |  DOF f/%.1f", 1.0 / 0.018);
    snprintf(line3, sizeof line3, "8 spheres  |  glass IOR 1.5  |  glossy");
    snprintf(line4, sizeof line4, "%.0f ms", ms);

    int lw[4], lh[4];
    SDL_Texture* lt[4];
    lt[0] = make_text(ren, font, line1, white, lw[0], lh[0]);
    lt[1] = make_text(ren, font, line2, dim,   lw[1], lh[1]);
    lt[2] = make_text(ren, font, line3, dim,   lw[2], lh[2]);
    lt[3] = make_text(ren, font, line4, white, lw[3], lh[3]);

    int pad_x = 14, pad_y = 12, line_gap = 4, bg_w = 0, bg_h = 0;
    for (int i = 0; i < 4; i++) {
        if (lw[i] > bg_w) bg_w = lw[i];
        bg_h += lh[i] + line_gap;
    }
    bg_w += pad_x * 2;
    bg_h += pad_y * 2 - line_gap + 6;

    bool run = true;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                run = false;
        }

        SDL_RenderCopy(ren, tex, nullptr, nullptr);

        // Dark semi-transparent background
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 10, 10, 12, 180);
        SDL_Rect bg = {8, 8, bg_w, bg_h};
        SDL_RenderFillRect(ren, &bg);

        // Text lines
        int yy = 8 + pad_y;
        for (int i = 0; i < 4; i++) {
            if (lt[i]) {
                SDL_Rect r = {8 + pad_x, yy, lw[i], lh[i]};
                SDL_RenderCopy(ren, lt[i], nullptr, &r);
            }
            yy += lh[i] + line_gap;
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    for (int i = 0; i < 4; i++) if (lt[i]) SDL_DestroyTexture(lt[i]);
    TTF_CloseFont(font);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
