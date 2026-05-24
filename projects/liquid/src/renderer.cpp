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

            // 3D height-field normal from density gradient
            int gi1 = std::max(gi - 1, 1);
            int gi2 = std::min(gi + 1, sol.N);
            int gj1 = std::max(gj - 1, 1);
            int gj2 = std::min(gj + 1, sol.N);
            float dl = sol.dens[sol.IX(gi1, gj)];
            float dr = sol.dens[sol.IX(gi2, gj)];
            float du = sol.dens[sol.IX(gi, gj1)];
            float dd = sol.dens[sol.IX(gi, gj2)];

            float nx = (dl - dr) * 0.8f;
            float ny = (du - dd) * 0.8f;
            float nz = 1.0f;
            float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nl > 0) { nx /= nl; ny /= nl; nz /= nl; }

            // Light from upper-right
            float lx = 0.3f, ly = -0.4f, lz = 0.8f;
            float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
            lx /= ll; ly /= ll; lz /= ll;

            float diff = std::max(0.0f, nx * lx + ny * ly + nz * lz);

            // Blinn-Phong specular
            float hx = lx, hy = ly, hz = lz + 1.0f;
            float hlen = std::sqrt(hx * hx + hy * hy + hz * hz);
            hx /= hlen; hy /= hlen; hz /= hlen;
            float spec = std::pow(std::max(0.0f, nx * hx + ny * hy + nz * hz), 16.0f);

            float ambient = 0.45f;
            float light = ambient + diff * 0.55f + spec * 0.3f;

            // Color from red/blue dye ratio
            float dr_dye = sol.dens_r[sol.IX(gi, gj)];
            float db_dye = sol.dens_b[sol.IX(gi, gj)];
            float total_dye = dr_dye + db_dye;
            float ratio = total_dye > 0 ? dr_dye / total_dye : 0.5f;
            float base_r = 40.0f  + ratio * 180.0f;
            float base_g = 60.0f  + (1.0f - ratio) * 30.0f;
            float base_b = 200.0f - ratio * 170.0f;

            uint8_t pr = (uint8_t)std::min(255.0f, base_r * light + spec * 180.0f);
            uint8_t pg = (uint8_t)std::min(255.0f, base_g * light + spec * 180.0f);
            uint8_t pb = (uint8_t)std::min(255.0f, base_b * light + spec * 180.0f);
            uint8_t pa = (uint8_t)(std::min(d / 0.12f, 1.0f) * 255);

            m_pix[j * m_texW + i] = (pa << 24) | (pr << 16) | (pg << 8) | pb;
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
