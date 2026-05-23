#include "renderer.hpp"
#include "physics.hpp"
#include <sstream>
#include <string>
#include <cmath>

Renderer::Renderer(SDL_Renderer* ren, int width, int height)
    : ren(ren), w(width), h(height) {
    TTF_Init();
    font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 16);
    fontSmall = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 13);
}

Renderer::~Renderer() {
    if (font) TTF_CloseFont(font);
    if (fontSmall) TTF_CloseFont(fontSmall);
    TTF_Quit();
}

void Renderer::drawText(int x, int y, const char* text, SDL_Color color, TTF_Font* f) {
    if (!f) return;
    SDL_Surface* surface = TTF_RenderText_Blended(f, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(ren, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(ren, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void Renderer::draw(const Simulation& sim, double cx, double cy, double zoom) {
    drawBackground();
    drawGrid(sim, cx, cy, zoom);
    drawGravWaves(sim, cx, cy, zoom);
    drawStars(sim, cx, cy, zoom);
    drawParticles(sim, cx, cy, zoom);
    drawTrails(sim, cx, cy, zoom);
    drawBlackHoles(sim, cx, cy, zoom);
    drawHUD(sim);
}

int Renderer::sx(double wx, double cx, double zm) const {
    return (int)((wx - cx) * zm + w / 2.0);
}

int Renderer::sy(double wy, double cy, double zm) const {
    return (int)((wy - cy) * zm + h / 2.0);
}

void Renderer::filledCircle(int cx, int cy, int r, Uint8 r2, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(ren, r2, g, b, a);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r)
                SDL_RenderDrawPoint(ren, cx + dx, cy + dy);
        }
    }
}

void Renderer::drawBackground() {
    SDL_SetRenderDrawColor(ren, 15, 20, 42, 255);
    SDL_RenderClear(ren);
}

void Renderer::drawGrid(const Simulation& sim, double cx, double cy, double zoom) {
    if (!sim.showGrid) return;

    int halfExt = sim.config.gridExtent;
    int spacing = sim.config.gridSpacing;
    int cols = 2 * halfExt / spacing + 1;

    for (size_t i = 0; i < sim.grid.size(); i++) {
        int gx = i % cols;
        int gy = i / cols;

        auto& p = sim.grid[i];
        int ix = sx(p.distortPos.x, cx, zoom);
        int iy = sy(p.distortPos.y, cy, zoom);
        if (ix < -100 || ix > w + 100 || iy < -100 || iy > h + 100) continue;

        double distAmt = (p.distortPos - p.origPos).len();
        distAmt = clamp(distAmt * 0.08, 0.0, 1.0);

        int alpha = (int)(10 + 20 * distAmt);
        if (alpha < 2) continue;

        if (gx < cols - 1) {
            auto& next = sim.grid[i + 1];
            int nx = sx(next.distortPos.x, cx, zoom);
            int ny = sy(next.distortPos.y, cy, zoom);
            SDL_SetRenderDrawColor(ren, 60, 100, 180, alpha);
            SDL_RenderDrawLine(ren, ix, iy, nx, ny);
        }

        if (gy < sim.grid.size() / cols - 1) {
            auto& next = sim.grid[i + cols];
            int nx = sx(next.distortPos.x, cx, zoom);
            int ny = sy(next.distortPos.y, cy, zoom);
            SDL_SetRenderDrawColor(ren, 60, 100, 180, alpha);
            SDL_RenderDrawLine(ren, ix, iy, nx, ny);
        }
    }
}

void Renderer::drawGravWaves(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& gw : sim.gravWaves) {
        double age = sim.time - gw.startTime;
        double expandSpeed = 20.0;
        double radius = age * expandSpeed * zoom;
        if (radius < 5.0 || radius > std::max(w, h) * 1.5) continue;

        int ox = sx(gw.origin.x, cx, zoom);
        int oy = sy(gw.origin.y, cy, zoom);
        int alpha = (int)clamp(gw.amplitude * 200, 0.0, 200.0);

        for (int ri = -3; ri <= 3; ri++) {
            double r = radius + ri * 4.0;
            if (r < 0) continue;
            int ir = (int)r;
            int ringAlpha = (int)(alpha * (1.0 - std::abs(ri) / 4.0));
            if (ringAlpha < 5) continue;

            SDL_SetRenderDrawColor(ren, 100, 180, 255, ringAlpha);
            for (int dy = -ir; dy <= ir; dy++) {
                for (int dx = -ir; dx <= ir; dx++) {
                    double d = std::sqrt(dx*dx + dy*dy);
                    if (d > r - 1.5 && d <= r + 1.5) {
                        SDL_RenderDrawPoint(ren, ox + dx, oy + dy);
                    }
                }
            }
        }
    }
}

void Renderer::drawStars(const Simulation& sim, double cx, double cy, double zoom) {
    static const struct { Uint8 r, g, b; } starColors[] = {
        {180, 200, 255},
        {255, 220, 180},
        {200, 255, 200}
    };

    for (auto& s : sim.stars) {
        Vec2 lensed = s.pos;
        for (auto& bh : sim.blackHoles) {
            if (bh.merged && bh.mergeProgress >= 1.0) continue;
            Vec2 diff = lensed - bh.pos;
            double r2 = diff.len2();
            if (r2 < 1.0) r2 = 1.0;
            double deflection = 8.0 * bh.mass / r2;
            lensed = lensed + diff * deflection;
        }

        int ix = sx(lensed.x, cx, zoom);
        int iy = sy(lensed.y, cy, zoom);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h) continue;

        double twinkle = 0.7 + 0.3 * std::sin(sim.time * 2.0 + s.twinklePhase);
        int alpha = (int)clamp(s.brightness * twinkle * 255, 0.0, 255.0);

        auto& sc = starColors[s.colorIdx % 3];
        SDL_SetRenderDrawColor(ren, sc.r, sc.g, sc.b, alpha);
        SDL_RenderDrawPoint(ren, ix, iy);

        if (s.brightness > 0.6) {
            if (ix + 1 < w) SDL_RenderDrawPoint(ren, ix + 1, iy);
            if (iy + 1 < h) SDL_RenderDrawPoint(ren, ix, iy + 1);
            if (ix + 1 < w && iy + 1 < h) SDL_RenderDrawPoint(ren, ix + 1, iy + 1);
        }
    }
}

void Renderer::drawParticles(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& p : sim.particles) {
        int ix = sx(p.pos.x, cx, zoom);
        int iy = sy(p.pos.y, cy, zoom);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h) continue;

        int alpha = (int)(p.a * (p.lifetime / p.maxLife));
        if (alpha < 0) alpha = 0;
        SDL_SetRenderDrawColor(ren, p.r, p.g, p.b, alpha);
        SDL_RenderDrawPoint(ren, ix, iy);
        if (ix + 1 < w && iy + 1 < h) {
            SDL_RenderDrawPoint(ren, ix + 1, iy);
            SDL_RenderDrawPoint(ren, ix, iy + 1);
        }
    }
}

void Renderer::drawTrails(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& bh : sim.blackHoles) {
        if (bh.merged || bh.trail.size() < 2) continue;

        for (size_t i = 1; i < bh.trail.size(); i++) {
            float t = (float)i / bh.trail.size();
            int alpha = (int)(120 * t * t);
            if (alpha < 3) continue;

            int px = sx(bh.trail[i].x, cx, zoom);
            int py = sy(bh.trail[i].y, cy, zoom);
            if (px < -10 || px > w + 10 || py < -10 || py > h + 10) continue;

            SDL_SetRenderDrawColor(ren, 180, 120, 255, alpha);
            SDL_RenderDrawPoint(ren, px, py);
        }
    }
}

void Renderer::drawBlackHoles(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& bh : sim.blackHoles) {
        if (bh.merged && bh.mergeProgress >= 1.0) continue;

        int bx = sx(bh.pos.x, cx, zoom);
        int by = sy(bh.pos.y, cy, zoom);
        double visR = bh.radius * zoom;
        if (visR < 2.0) visR = 2.0;

        filledCircle(bx, by, (int)(visR * 2.0), 60, 30, 140, 40);
        filledCircle(bx, by, (int)(visR * 1.4), 100, 50, 200, 25);

        for (int ri = 0; ri < 5; ri++) {
            double ringR = visR * (1.5 + ri * 0.45 + 0.25 * std::sin(sim.time * 3.0 + ri * 1.3));
            if (ringR > 600) break;
            int ir = (int)ringR;
            int alpha = (int)(120 - ri * 18);
            if (alpha < 15) alpha = 15;

            for (int dy = -ir; dy <= ir; dy++) {
                for (int dx = -ir; dx <= ir; dx++) {
                    double d = std::sqrt(dx*dx + dy*dy);
                    if (d > ringR - 1.8 && d <= ringR) {
                        double angle = std::atan2(dy, dx);
                        double density = 0.3 + 0.7 * (0.5 + 0.5 * std::sin(angle * 6.0 + sim.time * 4.0 + ri * 2.0));
                        if (density > 0.4) {
                            SDL_SetRenderDrawColor(ren, 255, 180, 60, alpha);
                            SDL_RenderDrawPoint(ren, bx + dx, by + dy);
                        }
                    }
                }
            }
        }

        int horizonR = (int)visR;
        SDL_SetRenderDrawColor(ren, 180, 120, 255, 160);
        for (int dy = -horizonR; dy <= horizonR; dy++) {
            for (int dx = -horizonR; dx <= horizonR; dx++) {
                double d = std::sqrt(dx*dx + dy*dy);
                if (d > visR - 2.0 && d <= visR) {
                    SDL_RenderDrawPoint(ren, bx + dx, by + dy);
                }
            }
        }

        int innerR = (int)(visR - 2.0);
        if (innerR > 0) {
            filledCircle(bx, by, innerR, 5, 5, 15, 255);
        }

        int coreR = (int)(visR * 0.35);
        if (coreR < 2) coreR = 2;
        for (int dy = -coreR; dy <= coreR; dy++) {
            for (int dx = -coreR; dx <= coreR; dx++) {
                double d = std::sqrt(dx*dx + dy*dy) / coreR;
                if (d <= 1.0) {
                    int alpha = (int)(255 * (1.0 - d * 0.7));
                    SDL_SetRenderDrawColor(ren, 220, 180, 255, alpha);
                    SDL_RenderDrawPoint(ren, bx + dx, by + dy);
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawPoint(ren, bx, by);
    }
}

void Renderer::drawHUD(const Simulation& sim) {
    // Merger flash
    if (sim.merged) {
        double flash = 180.0 * std::exp(-(sim.time - sim.mergerTime) * 0.6);
        if (flash > 2.0) {
            SDL_SetRenderDrawColor(ren, 255, 255, 255, (int)flash);
            SDL_RenderFillRect(ren, nullptr);
        }
        for (auto& gw : sim.gravWaves) {
            double age = sim.time - gw.startTime;
            if (age < 0.15) {
                int flashAlpha = (int)(180 * (1.0 - age / 0.15));
                SDL_SetRenderDrawColor(ren, 160, 200, 255, flashAlpha);
                SDL_RenderFillRect(ren, nullptr);
            }
        }
    }

    SDL_Color white = {220, 220, 220, 255};
    SDL_Color accent = {160, 200, 255, 255};
    SDL_Color dim = {150, 150, 150, 255};

    // === TOP STATUS BAR ===
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_Rect topBg = {0, 0, w, 26};
    SDL_RenderFillRect(ren, &topBg);

    std::string status;
    status += "Time: " + std::to_string((int)(sim.time * 10) / 10.0) + "s";
    status += "  |  Speed: " + std::to_string((int)(sim.speed * 100)) + "%";
    if (sim.paused) status += "  |  [PAUSED]";
    if (sim.merged) status += "  |  [MERGED]";
    drawText(10, 4, status.c_str(), white, fontSmall);

    int rightX = w - 10;
    std::string rightStatus = std::to_string(sim.blackHoles.size()) + " BH  |  " +
                              std::to_string(sim.particles.size()) + " particles";
    drawText(rightX - (int)(rightStatus.length() * 7), 4, rightStatus.c_str(), dim, fontSmall);

    // === BOTTOM CONTROLS BAR ===
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
    SDL_Rect botBg = {0, h - 30, w, 30};
    SDL_RenderFillRect(ren, &botBg);

    const char* controls =
        "Space:Pause  R:Reset  G:Grid  F:Frame  Scroll:Zoom  Drag:Pan  Right-Click:Add BH  ESC:Quit";
    drawText(10, h - 24, controls, accent, fontSmall);

    // Grid indicator
    if (sim.showGrid) {
        drawText(w - 50, h - 24, "Grid:ON", dim, fontSmall);
    } else {
        drawText(w - 55, h - 24, "Grid:OFF", dim, fontSmall);
    }
}
