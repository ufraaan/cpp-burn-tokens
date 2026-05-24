#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include "sph.hpp"
#include "renderer.hpp"

int main() {
    std::srand(std::time(nullptr));

    const int WIN_W = 800, WIN_H = 600;
    const int SIM_W = 780, SIM_H = 580;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* win = SDL_CreateWindow("Liquid — SPH Fluid Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SPHParams params;
    params.h = 18.0f;
    params.h2 = params.h * params.h;
    params.poly6Const = 315.0f / (64.0f * M_PI * std::pow(params.h, 9));
    params.spikyGradConst = -45.0f / (M_PI * std::pow(params.h, 6));
    params.viscLapConst = 45.0f / (M_PI * std::pow(params.h, 6));
    params.restDensity = 250.0f;
    params.gasConst = 0.5f;
    params.viscosity = 0.15f;
    params.gravity = 200.0f;
    params.dt = 0.003f;
    params.subSteps = 4;
    params.wallDamping = 0.3f;

    SPHSolver solver(SIM_W, SIM_H, params);
    solver.addBlock(100, 100, 14);

    FluidRenderer renderer(ren, WIN_W, WIN_H);

    bool running = true;
    int lastMX = 0, lastMY = 0;
    bool mouseDown = false;
    Uint32 lastTick = SDL_GetTicks();
    float spawnTimer = 0;
    int fps = 0, frameCount = 0;
    Uint32 fpsTick = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_r:
                        solver.reset();
                        solver.addBlock(100, 100, 14);
                        spawnTimer = 0;
                        break;
                    case SDLK_SPACE:
                        solver.addBlock(300 + rand() % 200, 80, 6);
                        break;
                    case SDLK_g:
                        params.gravity = (params.gravity == 0) ? 200.0f : 0.0f;
                        solver = SPHSolver(SIM_W, SIM_H, params);
                        solver.addBlock(100, 100, 14);
                        break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                mouseDown = true;
                lastMX = e.button.x;
                lastMY = e.button.y;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                mouseDown = false;
            if (e.type == SDL_MOUSEMOTION && mouseDown) {
                lastMX = e.motion.x;
                lastMY = e.motion.y;
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTick = now;

        if (mouseDown) {
            spawnTimer += dt;
            while (spawnTimer > 0.01f) {
                float sx = lastMX + (rand() % 30 - 15);
                float sy = lastMY + (rand() % 20 - 10);
                solver.addParticle(sx, sy,
                    (rand() % 60 - 30) * 0.5f,
                    -100 + (rand() % 80) * -0.5f);
                spawnTimer -= 0.01f;
            }
        } else {
            spawnTimer = 0;
        }

        solver.update();
        renderer.present(solver);
        SDL_RenderPresent(ren);

        frameCount++;
        if (now - fpsTick > 1000) {
            fps = frameCount;
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "Liquid [%d FPS | %zu particles] Click=spray  R=reset  Space=splash  G=gravity",
                fps, solver.particles().size());
            SDL_SetWindowTitle(win, buf);
            frameCount = 0;
            fpsTick = now;
        }

        Uint32 frameTime = SDL_GetTicks() - now;
        if (frameTime < 16) SDL_Delay(16 - frameTime);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
