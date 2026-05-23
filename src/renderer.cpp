#include "renderer.hpp"
#include "physics.hpp"
#include <sstream>
#include <string>
#include <cmath>
#include <cstring>

Renderer::Renderer(SDL_Renderer* ren, int width, int height) : ren(ren), w(width), h(height) {
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
    drawJets(sim, cx, cy, zoom);
    drawKilonova(sim, cx, cy, zoom);
    drawParticles(sim, cx, cy, zoom);
    drawTrails(sim, cx, cy, zoom);
    drawBlackHoles(sim, cx, cy, zoom);
    drawSpectrogram(sim);
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
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r)
                SDL_RenderDrawPoint(ren, cx + dx, cy + dy);
}

void Renderer::drawBackground() {
    // Dark space gradient
    SDL_SetRenderDrawColor(ren, 8, 10, 28, 255);
    SDL_RenderClear(ren);
    // Subtle nebula tint near the bottom
    SDL_SetRenderDrawColor(ren, 15, 8, 30, 60);
    SDL_Rect nebula = {0, h * 2 / 3, w, h / 3};
    SDL_RenderFillRect(ren, &nebula);
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

        double distAmt = (p.distortPos - p.origPos).len() * 0.08;
        distAmt = clamp(distAmt, 0.0, 1.0);
        int alpha = (int)(8 + 18 * distAmt);
        if (alpha < 2) continue;

        if (gx < cols - 1) {
            auto& next = sim.grid[i + 1];
            SDL_SetRenderDrawColor(ren, 50, 80, 160, alpha);
            SDL_RenderDrawLine(ren, ix, iy, sx(next.distortPos.x, cx, zoom), sy(next.distortPos.y, cy, zoom));
        }
        if (gy < sim.grid.size() / cols - 1) {
            auto& next = sim.grid[i + cols];
            SDL_SetRenderDrawColor(ren, 50, 80, 160, alpha);
            SDL_RenderDrawLine(ren, ix, iy, sx(next.distortPos.x, cx, zoom), sy(next.distortPos.y, cy, zoom));
        }
    }
}

void Renderer::drawGravWaves(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& gw : sim.gravWaves) {
        double age = sim.time - gw.startTime;
        double radius = age * 20.0 * zoom;
        if (radius < 5.0 || radius > std::max(w, h) * 1.5) continue;

        int ox = sx(gw.origin.x, cx, zoom);
        int oy = sy(gw.origin.y, cy, zoom);
        int baseAlpha = (int)clamp(gw.amplitude * 180, 0.0, 180.0);

        for (int ri = -3; ri <= 3; ri++) {
            double r = radius + ri * 4.0;
            if (r < 0) continue;
            int ir = (int)r;
            int ringAlpha = (int)(baseAlpha * (1.0 - std::abs(ri) / 4.0));
            if (ringAlpha < 5) continue;

            SDL_SetRenderDrawColor(ren, 80, 160, 255, ringAlpha);
            for (int dy = -ir; dy <= ir; dy++)
                for (int dx = -ir; dx <= ir; dx++) {
                    double d = std::sqrt(dx*dx + dy*dy);
                    if (d > r - 1.5 && d <= r + 1.5)
                        SDL_RenderDrawPoint(ren, ox + dx, oy + dy);
                }
        }
    }
}

void Renderer::drawStars(const Simulation& sim, double cx, double cy, double zoom) {
    static const struct { Uint8 r, g, b; } colors[] = {
        {180, 200, 255}, {255, 220, 180}, {200, 240, 200}
    };

    for (auto& s : sim.stars) {
        Vec2 lensed = s.pos;
        for (auto& bh : sim.blackHoles) {
            if (bh.merged && bh.mergeProgress >= 1.0) continue;
            Vec2 diff = lensed - bh.pos;
            double r2 = diff.len2();
            if (r2 < 1.0) r2 = 1.0;
            lensed = lensed + diff * (8.0 * bh.mass / r2);
        }

        int ix = sx(lensed.x, cx, zoom);
        int iy = sy(lensed.y, cy, zoom);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h) continue;

        double twinkle = 0.7 + 0.3 * std::sin(sim.time * 2.0 + s.twinklePhase);
        int alpha = (int)clamp(s.brightness * twinkle * 235, 0.0, 235.0);
        auto& c = colors[s.colorIdx % 3];
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, alpha);
        SDL_RenderDrawPoint(ren, ix, iy);

        if (s.brightness > 0.65) {
            if (ix + 1 < w) SDL_RenderDrawPoint(ren, ix + 1, iy);
            if (iy + 1 < h) SDL_RenderDrawPoint(ren, ix, iy + 1);
        }
        if (s.brightness > 0.9) {
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, alpha / 3);
            if (ix - 1 >= 0) SDL_RenderDrawPoint(ren, ix - 1, iy);
            if (iy - 1 >= 0) SDL_RenderDrawPoint(ren, ix, iy - 1);
        }
    }
}

void Renderer::drawParticles(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& p : sim.particles) {
        int ix = sx(p.pos.x, cx, zoom);
        int iy = sy(p.pos.y, cy, zoom);
        if (ix < 0 || ix >= w || iy < 0 || iy >= h) continue;

        int alpha = (int)(p.a * (p.lifetime / p.maxLife));
        if (alpha <= 0) continue;
        SDL_SetRenderDrawColor(ren, p.r, p.g, p.b, alpha);
        SDL_RenderDrawPoint(ren, ix, iy);
        if (ix + 1 < w && iy + 1 < h) {
            SDL_RenderDrawPoint(ren, ix + 1, iy);
            SDL_RenderDrawPoint(ren, ix, iy + 1);
        }
    }
}

void Renderer::drawKilonova(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& kp : sim.kilonova) {
        int ix = sx(kp.pos.x, cx, zoom);
        int iy = sy(kp.pos.y, cy, zoom);
        if (ix < -10 || ix > w + 10 || iy < -10 || iy > h + 10) continue;

        double t = kp.lifetime / kp.maxLife;
        int alpha = (int)(255 * t * t);
        if (alpha < 2) continue;

        int size = (int)(2.0 + 4.0 * t);
        SDL_SetRenderDrawColor(ren, kp.r, kp.g, kp.b, alpha);
        for (int dy = -size; dy <= size; dy++)
            for (int dx = -size; dx <= size; dx++) {
                double d = std::sqrt(dx*dx + dy*dy);
                if (d <= size && ix + dx >= 0 && ix + dx < w && iy + dy >= 0 && iy + dy < h) {
                    int la = (int)(alpha * (1.0 - d / size));
                    SDL_SetRenderDrawColor(ren, kp.r, kp.g, kp.b, la);
                    SDL_RenderDrawPoint(ren, ix + dx, iy + dy);
                }
            }
    }
}

void Renderer::drawJets(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& jp : sim.jets) {
        int ix = sx(jp.pos.x, cx, zoom);
        int iy = sy(jp.pos.y, cy, zoom);
        if (ix < -20 || ix > w + 20 || iy < -20 || iy > h + 20) continue;

        double t = jp.lifetime / jp.maxLife;
        int alpha = (int)(180 * t);
        if (alpha < 3) continue;

        int size = (int)(1.0 + 3.0 * t);
        SDL_SetRenderDrawColor(ren, jp.r, jp.g, jp.b, alpha);
        for (int dy = -size; dy <= size; dy++)
            for (int dx = -size; dx <= size; dx++)
                if (std::sqrt((double)(dx*dx + dy*dy)) <= size)
                    if (ix + dx >= 0 && ix + dx < w && iy + dy >= 0 && iy + dy < h)
                        SDL_RenderDrawPoint(ren, ix + dx, iy + dy);
    }
}

void Renderer::drawTrails(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& bh : sim.blackHoles) {
        if (bh.merged || bh.trail.size() < 2) continue;
        for (size_t i = 1; i < bh.trail.size(); i++) {
            float t = (float)i / bh.trail.size();
            int alpha = (int)(140 * t * t);
            if (alpha < 3) continue;
            int px = sx(bh.trail[i].x, cx, zoom);
            int py = sy(bh.trail[i].y, cy, zoom);
            if (px < -10 || px > w + 10 || py < -10 || py > h + 10) continue;
            SDL_SetRenderDrawColor(ren, 200, 140, 255, alpha);
            SDL_RenderDrawPoint(ren, px, py);
            if (px + 1 < w) SDL_RenderDrawPoint(ren, px + 1, py);
            if (py + 1 < h) SDL_RenderDrawPoint(ren, px, py + 1);
        }
    }
}

// ─── Accretion Disk ──────────────────────────────────────────────────────────
void Renderer::drawAccretionDisk(int bx, int by, double visR, double time, double dopplerAngle) {
    double innerR = visR * 1.5;
    double outerR = visR * 4.5;
    double tilt = 0.55;
    int oR = (int)outerR + 1;

    for (int dy = -oR; dy <= oR; dy++) {
        double dyT = dy / tilt;
        double dyT2 = dyT * dyT;
        if (dyT2 > outerR * outerR) continue;

        double halfW = std::sqrt(outerR * outerR - dyT2);

        for (int dx = (int)-halfW; dx <= (int)halfW; dx++) {
            double r = std::sqrt(dx*dx + dyT2);
            if (r < innerR || r > outerR) continue;

            double angle = std::atan2(dyT, dx);
            double normR = (r - innerR) / (outerR - innerR);

            // Temperature: hot in, cool out
            double temp = 1.0 - normR * normR;

            // Doppler beaming: approaching side brighter
            double doppler = 1.0 + 0.5 * std::sin(angle - time * 2.0 - dopplerAngle);

            // Intensity with falloff at edges
            double edgeFade = 1.0;
            if (normR > 0.85) edgeFade = 1.0 - (normR - 0.85) / 0.15;
            if (normR < 0.08) edgeFade = normR / 0.08;

            double intensity = clamp(255.0 * temp * doppler * edgeFade, 0.0, 255.0);

            // Color: white-hot center → orange → red → dim
            Uint8 rCol = (Uint8)intensity;
            Uint8 gCol = (Uint8)(intensity * (0.3 + 0.7 * temp));
            Uint8 bCol = (Uint8)(intensity * 0.15 * temp * temp);

            SDL_SetRenderDrawColor(ren, rCol, gCol, bCol, 255);
            SDL_RenderDrawPoint(ren, bx + dx, by + dy);
        }
    }
}

// ─── Photon Ring ─────────────────────────────────────────────────────────────
void Renderer::drawPhotonRing(int bx, int by, double visR, double time, double dopplerAngle) {
    double pr = visR * 1.5; // photon sphere radius
    int ir = (int)(pr + 3);

    for (int dy = -ir; dy <= ir; dy++) {
        for (int dx = -ir; dx <= ir; dx++) {
            double d = std::sqrt(dx*dx + dy*dy);
            if (d > pr - 2.5 && d <= pr + 2.5) {
                double angle = std::atan2(dy, dx);
                double bright = 0.5 + 0.5 * std::sin(angle - time * 3.0 - dopplerAngle);
                double falloff = 1.0 - std::abs(d - pr) / 2.5;

                // Inner edge: orange glow, outer edge: blue-white
                Uint8 rCol = (Uint8)(255 * falloff * (0.8 + 0.2 * bright));
                Uint8 gCol = (Uint8)(240 * falloff * (0.6 + 0.4 * bright));
                Uint8 bCol = (Uint8)(200 * falloff * (0.3 + 0.7 * bright));
                int alpha = (int)(240 * falloff);

                SDL_SetRenderDrawColor(ren, rCol, gCol, bCol, alpha);
                SDL_RenderDrawPoint(ren, bx + dx, by + dy);
            }
        }
    }
}

// ─── Einstein Ring ───────────────────────────────────────────────────────────
void Renderer::drawEinsteinRing(int bx, int by, double visR, double time) {
    double er = visR * 2.8;
    int ir = (int)(er + 2);

    double pulse = 0.7 + 0.3 * std::sin(time * 1.5);

    for (int dy = -ir; dy <= ir; dy++) {
        for (int dx = -ir; dx <= ir; dx++) {
            double d = std::sqrt(dx*dx + dy*dy);
            if (d > er - 1.0 && d <= er + 1.0) {
                double angle = std::atan2(dy, dx);
                double density = 0.3 + 0.7 * (0.5 + 0.5 * std::sin(angle * 8.0 + time));
                int alpha = (int)(40 * pulse * density);
                if (alpha > 5) {
                    SDL_SetRenderDrawColor(ren, 100, 180, 255, alpha);
                    SDL_RenderDrawPoint(ren, bx + dx, by + dy);
                }
            }
        }
    }
}

// ─── Black Holes ─────────────────────────────────────────────────────────────
void Renderer::drawBlackHoles(const Simulation& sim, double cx, double cy, double zoom) {
    for (auto& bh : sim.blackHoles) {
        if (bh.merged && bh.mergeProgress >= 1.0) continue;

        int bx = sx(bh.pos.x, cx, zoom);
        int by = sy(bh.pos.y, cy, zoom);
        double visR = bh.radius * zoom;
        if (visR < 2.0) visR = 2.0;

        double dopplerAngle = bh.dopplerAngle;

        // Einstein ring (faint lensing ring)
        drawEinsteinRing(bx, by, visR, sim.time);

        // 3D accretion disk with Doppler beaming
        drawAccretionDisk(bx, by, visR, sim.time, dopplerAngle);

        // Outer purple glow
        filledCircle(bx, by, (int)(visR * 2.2), 50, 20, 120, 18);
        filledCircle(bx, by, (int)(visR * 1.6), 80, 30, 180, 15);

        // Photon ring (bright orange/white)
        drawPhotonRing(bx, by, visR, sim.time, dopplerAngle);

        // Event horizon shadow
        int horizonR = (int)(visR - 0.5);
        if (horizonR > 0) {
            filledCircle(bx, by, horizonR, 3, 3, 12, 255);
        }

        // Relativistic jet glow (polar direction indicators)
        if (!bh.merged) {
            double jetAngle = dopplerAngle + 1.5708;
            for (int j = 0; j < 2; j++) {
                double ja = jetAngle + j * 3.14159;
                int jx = bx + (int)(visR * 1.3 * std::cos(ja));
                int jy = by + (int)(visR * 1.3 * std::sin(ja));
                if (jx >= 0 && jx < w && jy >= 0 && jy < h) {
                    SDL_SetRenderDrawColor(ren, 200, 150, 255, 60);
                    SDL_RenderDrawPoint(ren, jx, jy);
                }
            }
        }

        // Singularity glow
        int coreR = (int)(visR * 0.3);
        if (coreR < 1) coreR = 1;
        for (int dy = -coreR; dy <= coreR; dy++)
            for (int dx = -coreR; dx <= coreR; dx++) {
                double d = std::sqrt(dx*dx + dy*dy) / coreR;
                if (d <= 1.0) {
                    int alpha = (int)(255 * (1.0 - d * 0.7));
                    SDL_SetRenderDrawColor(ren, 230, 190, 255, alpha);
                    SDL_RenderDrawPoint(ren, bx + dx, by + dy);
                }
            }

        // Central bright dot
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawPoint(ren, bx, by);
        if (bx + 1 < w) SDL_RenderDrawPoint(ren, bx + 1, by);
        if (by + 1 < h) SDL_RenderDrawPoint(ren, bx, by + 1);
    }
}

// ─── Spectrogram ──────────────────────────────────────────────────────────────
void Renderer::drawSpectrogram(const Simulation& sim) {
    if (!sim.showSpectrogram) return;
    if (sim.freqHistory.size() < 2) return;

    int graphX = 10;
    int graphY = h - 120;
    int graphW = w - 20;
    int graphH = 70;
    if (!sim.showGrid) graphY = h - 100;

    // Background
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_Rect bg = {graphX - 5, graphY - 5, graphW + 10, graphH + 15};
    SDL_RenderFillRect(ren, &bg);

    // Border
    SDL_SetRenderDrawColor(ren, 60, 60, 100, 100);
    SDL_RenderDrawRect(ren, &bg);

    // Label
    SDL_Color dim = {130, 150, 180, 255};
    drawText(graphX, graphY - 2, "GW Frequency", dim, fontSmall);

    // Find max freq in view
    double maxFreq = 20.0;
    for (auto& fr : sim.freqHistory)
        if (fr.freq > maxFreq) maxFreq = fr.freq;
    if (maxFreq < 50) maxFreq = 50;

    double timeRange = sim.freqHistory.back().time - sim.freqHistory.front().time;
    if (timeRange < 0.01) return;

    // Draw waveform
    for (size_t i = 1; i < sim.freqHistory.size(); i++) {
        double t0 = (sim.freqHistory[i - 1].time - sim.freqHistory.front().time) / timeRange;
        double t1 = (sim.freqHistory[i].time - sim.freqHistory.front().time) / timeRange;

        double h0 = sim.freqHistory[i - 1].freq / maxFreq;
        double h1 = sim.freqHistory[i].freq / maxFreq;

        int x0 = graphX + (int)(t0 * graphW);
        int x1 = graphX + (int)(t1 * graphW);
        int y0 = graphY + graphH - (int)(h0 * (graphH - 4)) - 2;
        int y1 = graphY + graphH - (int)(h1 * (graphH - 4)) - 2;

        // Color based on frequency
        double f = (sim.freqHistory[i].freq / maxFreq);
        Uint8 rCol = (Uint8)(80 + 175 * f);
        Uint8 gCol = (Uint8)(100 + 155 * (1.0 - f * 0.5));
        Uint8 bCol = (Uint8)(255 - 50 * f);

        SDL_SetRenderDrawColor(ren, rCol, gCol, bCol, 220);
        SDL_RenderDrawLine(ren, x0, y0, x1, y1);
    }

    // Current frequency value
    double curFreq = sim.freqHistory.back().freq;
    if (curFreq > 1.0) {
        std::string freqStr = std::to_string((int)curFreq) + " Hz";
        SDL_Color bright = {200, 220, 255, 255};
        drawText(graphX + graphW - 70, graphY - 2, freqStr.c_str(), bright, fontSmall);
    }
}

// ─── HUD ──────────────────────────────────────────────────────────────────────
void Renderer::drawHUD(const Simulation& sim) {
    SDL_Color white = {220, 220, 225, 255};
    SDL_Color accent = {150, 190, 255, 255};
    SDL_Color dim = {130, 140, 160, 255};

    // Merger flash
    if (sim.merged) {
        double flash = 200.0 * std::exp(-(sim.time - sim.mergerTime) * 0.5);
        if (flash > 2.0) {
            SDL_SetRenderDrawColor(ren, 255, 255, 255, (int)flash);
            SDL_RenderFillRect(ren, nullptr);
        }
        for (auto& gw : sim.gravWaves) {
            double age = sim.time - gw.startTime;
            if (age < 0.15) {
                int fa = (int)(180 * (1.0 - age / 0.15));
                SDL_SetRenderDrawColor(ren, 160, 200, 255, fa);
                SDL_RenderFillRect(ren, nullptr);
            }
        }
    }

    // Screen shake is applied externally (in main.cpp camera offset)

    // Top status bar
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
    SDL_Rect topBg = {0, 0, w, 26};
    SDL_RenderFillRect(ren, &topBg);

    std::string status;
    status += "Time: " + std::to_string((int)(sim.time * 10) / 10.0) + "s";
    status += "  |  Speed: " + std::to_string((int)(sim.speed * 100)) + "%";
    if (sim.paused) status += "  |  [PAUSED]";
    if (sim.merged) status += "  |  [MERGED]";
    drawText(10, 4, status.c_str(), white, fontSmall);

    std::string right = std::to_string(sim.blackHoles.size()) + " BH";
    right += "  |  " + std::to_string(sim.particles.size() + sim.kilonova.size() + sim.jets.size()) + " particles";
    drawText(w - 10 - (int)right.length() * 7, 4, right.c_str(), dim, fontSmall);

    // Bottom controls bar (if no spectrogram, show at very bottom)
    int barY = (sim.showSpectrogram) ? h - 160 : h - 36;

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 195);
    SDL_Rect ctrlBg = {0, barY - 4, w, 30};
    SDL_RenderFillRect(ren, &ctrlBg);

    const char* ctrl1 = "Space:Pause  R:Reset  G:Grid  F:Frame  S:GWave  Scroll:Zoom";
    const char* ctrl2 = "Drag:Pan  Right-Click:Add BH  ESC:Quit";
    drawText(10, barY, ctrl1, accent, fontSmall);
    drawText(10, barY + 14, ctrl2, accent, fontSmall);

    // Grid/spectrogram indicators
    std::string ind = "";
    if (sim.showGrid) ind += "Grid:ON  "; else ind += "Grid:OFF  ";
    if (sim.showSpectrogram) ind += "GWave:ON"; else ind += "GWave:OFF";
    drawText(w - 15 - (int)ind.length() * 7, barY + 14, ind.c_str(), dim, fontSmall);
}
