#include "renderer.hpp"
#include <algorithm>
#include <cmath>

static void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    h = std::fmod(h, 1.0f); if (h < 0) h += 1.0f;
    int i = (int)(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t = v * (1 - s * (1 - f));
    switch (i % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        case 5: r=v; g=p; b=q; break;
    }
}

FluidRenderer::FluidRenderer(SDL_Renderer* ren, int width, int height)
    : m_ren(ren), m_w(width), m_h(height), m_particleTex(nullptr)
{
    buildParticleTexture();
}

FluidRenderer::~FluidRenderer() {
    if (m_particleTex) SDL_DestroyTexture(m_particleTex);
}

void FluidRenderer::buildParticleTexture() {
    int size = 32;
    m_particleTex = SDL_CreateTexture(m_ren,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size, size);
    SDL_SetTextureBlendMode(m_particleTex, SDL_BLENDMODE_ADD);

    std::vector<Uint32> pixels(size * size, 0);
    int cx = size / 2, cy = size / 2;
    float maxR = size / 2;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float r = std::sqrt(dx * dx + dy * dy);
            if (r >= maxR) continue;
            float t = r / maxR;
            float alpha = (1 - t * t) * 255;
            Uint32 a = (Uint32)alpha;
            pixels[y * size + x] = (0xFF << 24) | (a << 16) | (a << 8) | a;
        }
    }

    SDL_UpdateTexture(m_particleTex, nullptr, pixels.data(), size * 4);
}

void FluidRenderer::present(const SPHSolver& solver) {
    // Background
    SDL_SetRenderDrawColor(m_ren, 8, 8, 18, 255);
    SDL_RenderClear(m_ren);

    // Container border
    SDL_SetRenderDrawColor(m_ren, 30, 35, 55, 255);
    SDL_Rect border = {2, 2, (int)solver.worldW() - 4, (int)solver.worldH() - 4};
    SDL_RenderDrawRect(m_ren, &border);

    const auto& particles = solver.particles();
    const auto& params = solver.params();

    // Draw particles
    int texSize = 32;
    for (const auto& p : particles) {
        float speed = p.vel.len();
        float hue = 0.55f - std::min(speed / 600.0f, 1.0f) * 0.15f;
        float sat = 0.8f;
        float val = 0.3f + std::min(speed / 400.0f, 1.0f) * 0.7f;

        float r, g, b;
        hsvToRgb(hue, sat, val, r, g, b);

        SDL_SetTextureColorMod(m_particleTex,
            (Uint8)(r * 255), (Uint8)(g * 255), (Uint8)(b * 255));
        SDL_SetTextureAlphaMod(m_particleTex, 200);

        SDL_Rect dst = {
            (int)(p.pos.x - texSize / 2),
            (int)(p.pos.y - texSize / 2),
            texSize, texSize
        };
        SDL_RenderCopy(m_ren, m_particleTex, nullptr, &dst);
    }
}
