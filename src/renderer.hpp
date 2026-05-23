#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "types.hpp"

class Simulation;

class Renderer {
public:
    Renderer(SDL_Renderer* ren, int width, int height);
    ~Renderer();
    void draw(const Simulation& sim, double camX, double camY, double zoom);

private:
    SDL_Renderer* ren;
    TTF_Font* font = nullptr;
    TTF_Font* fontSmall = nullptr;
    int w, h;

    void drawText(int x, int y, const char* text, SDL_Color color, TTF_Font* f);

    void drawBackground();
    void drawGrid(const Simulation& sim, double cx, double cy, double zoom);
    void drawGravWaves(const Simulation& sim, double cx, double cy, double zoom);
    void drawStars(const Simulation& sim, double cx, double cy, double zoom);
    void drawParticles(const Simulation& sim, double cx, double cy, double zoom);
    void drawKilonova(const Simulation& sim, double cx, double cy, double zoom);
    void drawJets(const Simulation& sim, double cx, double cy, double zoom);
    void drawTrails(const Simulation& sim, double cx, double cy, double zoom);
    void drawBlackHoles(const Simulation& sim, double cx, double cy, double zoom);
    void drawSpectrogram(const Simulation& sim);
    void drawHUD(const Simulation& sim);

    int sx(double wx, double cx, double zm) const;
    int sy(double wy, double cy, double zm) const;
    void filledCircle(int cx, int cy, int r, Uint8 r2, Uint8 g, Uint8 b, Uint8 a);
    void drawPhotonRing(int bx, int by, double visR, double time, double dopplerAngle);
    void drawAccretionDisk(int bx, int by, double visR, double time, double dopplerAngle);
    void drawEinsteinRing(int bx, int by, double visR, double time);
};
