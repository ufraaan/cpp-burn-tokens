#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include "fluid.hpp"

class Renderer {
public:
    Renderer(SDL_Renderer* ren, int n, int texW, int texH);
    ~Renderer();
    void present(FluidSolver& sol, int offX, int offY, int drawW, int drawH);

private:
    SDL_Renderer* m_ren;
    SDL_Texture* m_tex;
    std::vector<uint32_t> m_pix;
    int m_n, m_texW, m_texH;
};
