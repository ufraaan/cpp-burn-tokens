#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include "types.hpp"
#include "sph.hpp"

class FluidRenderer {
public:
    FluidRenderer(SDL_Renderer* ren, int width, int height);
    ~FluidRenderer();
    void present(const SPHSolver& solver);

private:
    void buildParticleTexture();
    SDL_Renderer* m_ren;
    SDL_Texture* m_particleTex;
    int m_w, m_h;
};
