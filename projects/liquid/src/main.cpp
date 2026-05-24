#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "fluid.hpp"
#include "renderer.hpp"
#include "hud.hpp"

int main() {
    std::srand(std::time(nullptr));

    const int WIN_W = 960, WIN_H = 960;
    const int N = 600;
    const int PAD = 30;
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

    sol.addDensity(N/2, N/3, 1.0f, 14);

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
                        sol.addDensity(N/2, N/3, 1.0f, 14);
                        break;
                    case SDLK_SPACE:
                        sol.addSplash(N/2 - 12 + rand()%24, N/3, 1.0f);
                        break;
                    case SDLK_g:
                        gravity = (gravity == 0) ? 2.0f : 0.0f;
                        break;
                    default: break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_CaptureMouse(SDL_TRUE);
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
                    sol.addDensityLine(gx, gy, ngx, ngy, 0.4f, 4);
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

        rend.present(sol, PAD, PAD, CONT_W, CONT_H);

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
