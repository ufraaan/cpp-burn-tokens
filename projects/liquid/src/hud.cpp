#include "hud.hpp"

HUD::HUD(SDL_Renderer* ren, TTF_Font* font)
    : m_ren(ren), m_font(font), m_tex(nullptr), m_w(0), m_h(0) {}

HUD::~HUD() { if (m_tex) SDL_DestroyTexture(m_tex); }

void HUD::setText(const char* text) {
    if (m_tex) { SDL_DestroyTexture(m_tex); m_tex = nullptr; }
    SDL_Color col = {170, 175, 200, 255};
    SDL_Surface* surf = TTF_RenderText_Blended(m_font, text, col);
    if (!surf) return;
    m_w = surf->w; m_h = surf->h;
    m_tex = SDL_CreateTextureFromSurface(m_ren, surf);
    SDL_FreeSurface(surf);
}

void HUD::draw(int x, int y) {
    if (!m_tex) return;
    SDL_Rect dst = {x, y, m_w, m_h};
    SDL_RenderCopy(m_ren, m_tex, nullptr, &dst);
}

int HUD::width() const { return m_w; }
int HUD::height() const { return m_h; }
