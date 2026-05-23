#include <SDL2/SDL.h>
#include "physics.hpp"
#include "renderer.hpp"
#include "audio.hpp"
#include <cstdlib>
#include <ctime>
#include <string>
#include <cmath>

static Vec2 screenToWorld(int sx, int sy, double camX, double camY, double zoom, int w, int h) {
    return {(sx - w / 2.0) / zoom + camX, (sy - h / 2.0) / zoom + camY};
}

static void getAutoCamera(const Simulation& sim, double& cx, double& cy, double& zm, int w, int h) {
    cx = 0; cy = 0; double totalMass = 0;
    for (auto& bh : sim.blackHoles) {
        if (!bh.merged || bh.mergeProgress < 1.0) {
            cx += bh.pos.x * bh.mass;
            cy += bh.pos.y * bh.mass;
            totalMass += bh.mass;
        }
    }
    if (totalMass > 0) { cx /= totalMass; cy /= totalMass; }
    double maxDist = 1.0;
    for (auto& bh : sim.blackHoles) {
        if (!bh.merged || bh.mergeProgress < 1.0) {
            double d = (bh.pos - Vec2{cx, cy}).len();
            if (d > maxDist) maxDist = d;
        }
    }
    zm = std::min(w, h) / (maxDist * 4.0);
    if (zm > 30.0) zm = 30.0;
    if (zm < 0.5) zm = 0.5;
}

int main() {
    std::srand(std::time(nullptr));

    const int W_WIDTH = 1200;
    const int W_HEIGHT = 800;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_Window* window = SDL_CreateWindow("Black Hole Collision Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W_WIDTH, W_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Simulation sim;
    Renderer gfx(renderer, W_WIDTH, W_HEIGHT);
    Audio audio;

    // Camera state
    double camX, camY, zoom;
    getAutoCamera(sim, camX, camY, zoom, W_WIDTH, W_HEIGHT);
    double targetZoom = zoom;
    bool inSolarSystem = false;

    bool running = true;
    bool dragging = false;
    int dragStartX = 0, dragStartY = 0;
    double dragStartCamX = 0, dragStartCamY = 0;
    Uint32 lastFrame = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; }

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_SPACE:  sim.paused = !sim.paused; break;
                    case SDLK_r:      sim.reset(); break;
                    case SDLK_EQUALS:
                    case SDLK_PLUS:   sim.speed = clamp(sim.speed * 1.5, 0.1, 10.0); break;
                    case SDLK_MINUS:  sim.speed = clamp(sim.speed / 1.5, 0.1, 10.0); break;
                    case SDLK_g:      sim.showGrid = !sim.showGrid; break;
                    case SDLK_s:
                        sim.showSpectrogram = !sim.showSpectrogram; break;
                    case SDLK_f:
                        getAutoCamera(sim, camX, camY, zoom, W_WIDTH, W_HEIGHT);
                        targetZoom = zoom;
                        break;
                    case SDLK_1:
                        sim.switchScene(SceneMode::BlackHoles);
                        inSolarSystem = false;
                        getAutoCamera(sim, camX, camY, zoom, W_WIDTH, W_HEIGHT);
                        targetZoom = zoom;
                        break;
                    case SDLK_2:
                        sim.switchScene(SceneMode::SolarSystem);
                        inSolarSystem = true;
                        camX = 0; camY = 0;
                        zoom = 10.0; targetZoom = 10.0;
                        break;
                }
            }

            // Right-click: add black hole
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
                Vec2 worldPos = screenToWorld(e.button.x, e.button.y, camX, camY, zoom, W_WIDTH, W_HEIGHT);
                double mass = 5.0 + (double)(std::rand() % 10);
                double radius = 1.5 + mass * 0.15;
                Vec2 vel{(double)(std::rand() % 20 - 10) / 5.0, (double)(std::rand() % 20 - 10) / 5.0};
                sim.addBlackHole(worldPos, vel, mass, radius);
            }

            // Left-click: start drag to pan
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                dragStartX = e.button.x;
                dragStartY = e.button.y;
                dragStartCamX = camX;
                dragStartCamY = camY;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            }

            if (e.type == SDL_MOUSEMOTION && dragging) {
                double dx = (e.motion.x - dragStartX) / zoom;
                double dy = (e.motion.y - dragStartY) / zoom;
                camX = dragStartCamX - dx;
                camY = dragStartCamY - dy;
            }

            if (e.type == SDL_MOUSEWHEEL) {
                double factor = (e.wheel.y > 0) ? 1.15 : (1.0 / 1.15);
                targetZoom = clamp(targetZoom * factor, 0.5, 60.0);
            }
        }

        // Physics
        Uint32 now = SDL_GetTicks();
        double frameDt = (now - lastFrame) / 1000.0;
        if (frameDt > 0.05) frameDt = 0.05;
        lastFrame = now;

        sim.update(frameDt);

        // Smooth zoom
        zoom += (targetZoom - zoom) * 0.12;

        // Screen shake (offset from merger)
        double shakeX = 0, shakeY = 0;
        if (sim.shakeIntensity > 0.5) {
            double si = sim.shakeIntensity / zoom;
            shakeX = ((double)(std::rand() % 2000) / 1000.0 - 1.0) * si;
            shakeY = ((double)(std::rand() % 2000) / 1000.0 - 1.0) * si;
        }

        // Audio
        if (audio.isInitialized()) {
            double freq = sim.getOrbitalFrequency();
            audio.setFrequency(freq);
            double amp = 0.0;
            if (sim.merged) {
                double age = sim.time - sim.mergerTime;
                amp = 0.3 * std::exp(-age * 2.0);
            } else if (sim.blackHoles.size() >= 2) {
                int active = 0;
                for (auto& bh : sim.blackHoles) if (!bh.merged) active++;
                if (active >= 2) {
                    double minDist = 1e9;
                    for (size_t i = 0; i < sim.blackHoles.size(); i++)
                        for (size_t j = i + 1; j < sim.blackHoles.size(); j++)
                            if (!sim.blackHoles[i].merged && !sim.blackHoles[j].merged)
                                minDist = std::min(minDist, (sim.blackHoles[i].pos - sim.blackHoles[j].pos).len());
                    amp = clamp(0.3 / (minDist * 0.1 + 1.0), 0.0, 0.4);
                }
            }
            audio.setAmplitude(amp);
        }

        // Render with shake offset
        gfx.draw(sim, camX + shakeX, camY + shakeY, zoom);
        SDL_RenderPresent(renderer);

        // Window title
        std::string title = "Black Hole Collision";
        if (sim.paused) title += " [PAUSED]";
        if (sim.merged) title += " [MERGED]";
        title += " | Speed " + std::to_string((int)(sim.speed * 100)) + "%";
        title += " | " + std::to_string((int)(sim.time * 10) / 10.0) + "s";
        SDL_SetWindowTitle(window, title.c_str());

        // Frame cap
        Uint32 frameTime = SDL_GetTicks() - now;
        if (frameTime < 16) SDL_Delay(16 - frameTime);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
