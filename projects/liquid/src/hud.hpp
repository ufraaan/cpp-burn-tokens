#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class HUD {
public:
    HUD(SDL_Renderer* ren, TTF_Font* font);
    ~HUD();
    void setText(const char* text);
    void draw(int x, int y);
    int width() const;
    int height() const;

private:
    SDL_Renderer* m_ren;
    TTF_Font* m_font;
    SDL_Texture* m_tex;
    int m_w, m_h;
};
