#include "renderer.hpp"

Renderer::Renderer(SDL_Renderer* ren, int n, int texW, int texH)
    : m_ren(ren), m_n(n), m_texW(texW), m_texH(texH)
{
    m_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, texW, texH);
    m_pix.assign(texW * texH, 0);
}

Renderer::~Renderer() { if (m_tex) SDL_DestroyTexture(m_tex); }

void Renderer::present(FluidSolver& sol, int offX, int offY, int drawW, int drawH) {
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

            uint8_t r = (uint8_t)(15 + t * 110);
            uint8_t g = (uint8_t)(45 + t * 180);
            uint8_t b = (uint8_t)(130 + t * 125);
            uint8_t a = (uint8_t)(std::min(d / 0.12f, 1.0f) * 255);

            // Specular shimmer from density gradient
            float dx = sol.dens[sol.IX(std::min(gi+1,sol.N), gj)]
                     - sol.dens[sol.IX(std::max(gi-1,1), gj)];
            float dy = sol.dens[sol.IX(gi, std::min(gj+1,sol.N))]
                     - sol.dens[sol.IX(gi, std::max(gj-1,1))];
            float spec = std::abs(dx) + std::abs(dy);
            float sh = std::min(spec * 3.0f, 1.0f) * 0.35f;
            r = (uint8_t)std::min(255.0f, r + sh * 180);
            g = (uint8_t)std::min(255.0f, g + sh * 160);
            b = (uint8_t)std::min(255.0f, b + sh * 120);

            m_pix[j * m_texW + i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    SDL_UpdateTexture(m_tex, nullptr, m_pix.data(), m_texW * 4);

    SDL_SetRenderDrawColor(m_ren, 8, 8, 26, 255);
    SDL_RenderClear(m_ren);

    SDL_Rect dst = {offX, offY, drawW, drawH};
    SDL_RenderCopy(m_ren, m_tex, nullptr, &dst);

    SDL_SetRenderDrawColor(m_ren, 35, 40, 65, 255);
    SDL_Rect border = {offX - 1, offY - 1, drawW + 2, drawH + 2};
    SDL_RenderDrawRect(m_ren, &border);
}
