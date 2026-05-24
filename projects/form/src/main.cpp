#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// ─── 3D Math ────────────────────────────────────────────────────────────────
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(Vec3 o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(Vec3 o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(float s) const { return {x/s, y/s, z/s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(Vec3 o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    float dot(Vec3 o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3 cross(Vec3 o) const { return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x}; }
    float len2() const { return x*x + y*y + z*z; }
    float len() const { return std::sqrt(len2()); }
    Vec3 norm() const { float l = len(); return l > 0 ? *this / l : Vec3{0,0,0}; }
};

// ─── Mandelbulb Distance Estimator ──────────────────────────────────────────
struct Mandelbulb {
    int power;
    int maxIter;
    Mandelbulb(int p = 8, int iter = 10) : power(p), maxIter(iter) {}

    float de(Vec3 p) const {
        Vec3 z = p;
        float dr = 1.0f;
        float r = 0.0f;

        for (int i = 0; i < maxIter; i++) {
            r = z.len();
            if (r > 2.0f) break;

            float theta = std::acos(z.y / r);
            float phi = std::atan2(z.z, z.x);
            float pwr = (float)power;

            dr = std::pow(r, pwr - 1.0f) * pwr * dr + 1.0f;

            float zr = std::pow(r, pwr);
            theta *= pwr;
            phi *= pwr;

            z = Vec3(zr * std::sin(theta) * std::cos(phi),
                     zr * std::cos(theta),
                     zr * std::sin(theta) * std::sin(phi));
            z = z + p;
        }
        return 0.5f * std::log(r) * r / dr;
    }
};

// ─── Scene ──────────────────────────────────────────────────────────────────
struct Scene {
    Mandelbulb bulb{8, 10};
    float groundY = -1.5f;

    float de(Vec3 p) const {
        float gnd = p.y - groundY;
        float fractal = bulb.de(p);
        return std::min(gnd, fractal);
    }

    Vec3 normal(Vec3 p) const {
        float eps = 0.001f;
        Vec3 dx{eps, 0, 0}, dy{0, eps, 0}, dz{0, 0, eps};
        float d = de(p);
        return (Vec3{de(p+dx)-d, de(p+dy)-d, de(p+dz)-d}).norm();
    }
};

// ─── Camera ─────────────────────────────────────────────────────────────────
struct Camera {
    float theta = 1.2f;
    float phi = 0.0f;
    float dist = 4.0f;
    float targetTheta = 1.2f;
    float targetPhi = 0.0f;
    float targetDist = 4.0f;
    float fov = 1.2f;
    bool autoRotate = true;

    void update(float dt) {
        if (autoRotate) phi += dt * 0.15f;
        theta += (targetTheta - theta) * 0.08f;
        phi += (targetPhi - phi) * 0.08f;
        dist += (targetDist - dist) * 0.08f;
    }

    Vec3 pos() const {
        return Vec3(dist * std::sin(theta) * std::sin(phi),
                    dist * std::cos(theta),
                    dist * std::sin(theta) * std::cos(phi));
    }
};

// ─── Color ──────────────────────────────────────────────────────────────────
struct Color {
    float r, g, b;
    Color() : r(0), g(0), b(0) {}
    Color(float r, float g, float b) : r(r), g(g), b(b) {}
    Color operator+(Color o) const { return {r+o.r, g+o.g, b+o.b}; }
    Color operator-(Color o) const { return {r-o.r, g-o.g, b-o.b}; }
    Color operator*(float s) const { return {r*s, g*s, b*s}; }
    Color operator*(Color o) const { return {r*o.r, g*o.g, b*o.b}; }
    void saturate() { r = r>1?1:r<0?0:r; g = g>1?1:g<0?0:g; b = b>1?1:b<0?0:b; }
    Uint32 toRGBA() const {
        return ((Uint32)(r*255) << 16) | ((Uint32)(g*255) << 8) | ((Uint32)(b*255));
    }
};

Color lerp(Color a, Color b, float t) {
    return a + (b - a) * t;
}

// Orbit trap coloring for the fractal
Color fractalColor(Vec3 p, Vec3 normal, int iter, const Mandelbulb& bulb) {
    // Orbit trap: use the last position before escape
    float trap = std::abs(p.x) + std::abs(p.y) + std::abs(p.z);

    static const Color palette[] = {
        {0.1f, 0.0f, 0.2f},
        {0.2f, 0.0f, 0.5f},
        {0.5f, 0.1f, 0.8f},
        {0.9f, 0.2f, 0.4f},
        {1.0f, 0.5f, 0.1f},
        {1.0f, 0.8f, 0.2f},
        {1.0f, 1.0f, 0.6f},
    };

    float t = std::sin(trap * 3.0f + iter * 0.3f) * 0.5f + 0.5f;
    t = t * 6.0f;
    int i0 = (int)t;
    int i1 = std::min(i0 + 1, 6);
    float ft = t - i0;

    Color col = lerp(palette[i0 % 7], palette[i1 % 7], ft);

    // Add normal-based shading
    float diff = std::max(0.0f, normal.dot(Vec3{0.5f, 0.8f, 0.3f}.norm()));
    col = col * (0.3f + 0.7f * diff);

    // Add rim light
    return col;
}

// ─── Renderer ───────────────────────────────────────────────────────────────
void render(Uint32* pixels, int rw, int rh, int dw, int dh, const Camera& cam, const Scene& scene) {
    Vec3 camPos = cam.pos();
    Vec3 target{0, 0, 0};
    Vec3 forward = (target - camPos).norm();
    Vec3 right = Vec3{0, 1, 0}.cross(forward).norm();
    Vec3 up = forward.cross(right).norm();

    float aspect = (float)dw / dh;

    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int py = 0; py < rh; py++) {
        for (int px = 0; px < rw; px++) {
            // Map pixel to screen coordinates (anti-aliasing with 2 sub-samples)
            Color finalColor{0, 0, 0};
            int samples = 2;

            for (int sy = 0; sy < samples; sy++) {
                for (int sx = 0; sx < samples; sx++) {
                    float jx = ((float)px + (float)sx / samples) / rw * 2.0f - 1.0f;
                    float jy = ((float)py + (float)sy / samples) / rh * 2.0f - 1.0f;
                    jx *= aspect;

                    Vec3 dir = (forward + right * jx * std::tan(cam.fov * 0.5f)
                                        + up * jy * std::tan(cam.fov * 0.5f)).norm();

                    // Ray march
                    float t = 0.01f;
                    bool hit = false;
                    Vec3 hitPos;
                    int iter = 0;
                    constexpr int MAX_STEPS = 120;
                    constexpr float MAX_DIST = 20.0f;
                    constexpr float EPS = 0.0005f;

                    for (int step = 0; step < MAX_STEPS; step++) {
                        Vec3 pos = camPos + dir * t;
                        float d = scene.de(pos);
                        if (d < EPS) {
                            hit = true;
                            hitPos = pos;
                            // Estimate iteration count from position
                            float r = 0;
                            Vec3 z = pos;
                            for (int i = 0; i < scene.bulb.maxIter; i++) {
                                r = z.len();
                                if (r > 2.0f) { iter = i; break; }
                                float theta = std::acos(z.y / r);
                                float phi = std::atan2(z.z, z.x);
                                float pwr = (float)scene.bulb.power;
                                float zr = std::pow(r, pwr);
                                theta *= pwr;
                                phi *= pwr;
                                z = Vec3(zr * std::sin(theta) * std::cos(phi),
                                         zr * std::cos(theta),
                                         zr * std::sin(theta) * std::sin(phi));
                                z = z + pos;
                            }
                            break;
                        }
                        t += d * 0.7f;
                        if (t > MAX_DIST) break;
                    }

                    if (hit) {
                        Vec3 n = scene.normal(hitPos);
                        float ambient = 0.15f;

                        if (hitPos.y < -1.4f) {
                            // Ground plane
                            float check = (int)(hitPos.x * 4.0f + 1000) % 2 ^ (int)(hitPos.z * 4.0f + 1000) % 2;
                            float val = check ? 0.15f : 0.05f;
                            float diff = std::max(0.0f, n.dot(Vec3{0.5f, 1.0f, 0.3f}.norm()));
                            finalColor = Color{val, val, val} * (ambient + 0.85f * diff);

                            // Fog
                            float fog = std::min(1.0f, t / 8.0f);
                            finalColor = lerp(finalColor, Color{0.02f, 0.02f, 0.05f}, fog * fog);
                        } else {
                            finalColor = fractalColor(hitPos, n, iter, scene.bulb);

                            // Soft shadow (simple ambient occlusion)
                            float ao = 0.0f;
                            for (int i = 1; i <= 5; i++) {
                                float step = i * 0.03f;
                                Vec3 pos = hitPos + n * step;
                                float d = scene.de(pos);
                                ao += (step - d) / step;
                            }
                            float occlusion = std::max(0.0f, 1.0f - ao * 0.5f);
                            finalColor = finalColor * occlusion;

                            // Fog
                            float fog = std::min(1.0f, t / 10.0f);
                            finalColor = lerp(finalColor, Color{0.02f, 0.02f, 0.05f}, fog * fog * 0.7f);
                        }
                    } else {
                        // Background gradient
                        float t2 = dir.y * 0.5f + 0.5f;
                        finalColor = lerp(Color{0.01f, 0.01f, 0.03f}, Color{0.05f, 0.05f, 0.1f}, t2);
                    }

                    finalColor = finalColor * (1.0f / (samples * samples));
                }
            }

            // Copy to screen (bilinear upscale)
            int dx0 = px * dw / rw;
            int dy0 = py * dh / rh;
            int dx1 = (px + 1) * dw / rw;
            int dy1 = (py + 1) * dh / rh;

            Uint32 col = finalColor.toRGBA();
            for (int dy = dy0; dy < dy1; dy++)
                for (int dx = dx0; dx < dx1; dx++)
                    if (dx < dw && dy < dh)
                        pixels[dy * dw + dx] = col;
        }
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────
int main() {
    std::srand(std::time(nullptr));

    const int D_W = 960, D_H = 640;     // Display resolution
    const int R_W = 320, R_H = 213;     // Render resolution (1/3 scale)

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("Form — 3D Fractal Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, D_W, D_H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING, D_W, D_H);

    std::vector<Uint32> pixels(D_W * D_H);

    Camera cam;
    Scene scene;
    bool running = true;
    int lastMX = -1, lastMY = -1;
    bool mouseDown = false;
    Uint32 lastFrame = SDL_GetTicks();
    int frameCount = 0;
    Uint32 fpsTimer = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_SPACE: cam.autoRotate = !cam.autoRotate; break;
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                        scene.bulb.power = std::min(scene.bulb.power + 1, 12);
                        break;
                    case SDLK_MINUS:
                        scene.bulb.power = std::max(scene.bulb.power - 1, 2);
                        break;
                    case SDLK_UP:
                        scene.bulb.maxIter = std::min(scene.bulb.maxIter + 2, 30);
                        break;
                    case SDLK_DOWN:
                        scene.bulb.maxIter = std::max(scene.bulb.maxIter - 2, 4);
                        break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                mouseDown = true; lastMX = e.button.x; lastMY = e.button.y;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                mouseDown = false;
            if (e.type == SDL_MOUSEMOTION && mouseDown) {
                float dx = (float)(e.motion.x - lastMX) * 0.004f;
                float dy = (float)(e.motion.y - lastMY) * 0.004f;
                cam.targetPhi -= dx;
                cam.targetTheta += dy;
                cam.targetTheta = std::max(0.2f, std::min(2.8f, cam.targetTheta));
                lastMX = e.motion.x; lastMY = e.motion.y;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                cam.targetDist *= (e.wheel.y > 0) ? 0.85f : 1.15f;
                cam.targetDist = std::max(1.5f, std::min(20.0f, cam.targetDist));
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastFrame) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastFrame = now;

        cam.update(dt);

        // Render
        std::fill(pixels.begin(), pixels.end(), 0);
        render(pixels.data(), R_W, R_H, D_W, D_H, cam, scene);

        // Display
        SDL_UpdateTexture(tex, nullptr, pixels.data(), D_W * sizeof(Uint32));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);

        // Power/iteration indicator bar
        float pct = (float)(scene.bulb.power - 2) / 10.0f;
        int barX = D_W - 160, barY = 10, barW = 140, barH = 8;
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 30);
        SDL_Rect barBg = {barX, barY, barW, barH};
        SDL_RenderFillRect(ren, &barBg);
        SDL_SetRenderDrawColor(ren, 100, 180, 255, 160);
        SDL_Rect barFill = {barX, barY, (int)(barW * pct), barH};
        SDL_RenderFillRect(ren, &barFill);

        SDL_RenderPresent(ren);

        // Window title
        frameCount++;
        if (now - fpsTimer > 1000) {
            char title[128];
            std::snprintf(title, sizeof(title),
                "Form — %d FPS | Power %d  Iter %d   Orbits: %d | Drag to orbit | Scroll to zoom | +- power | up/down iter",
                frameCount, scene.bulb.power, scene.bulb.maxIter, R_W * R_H);
            SDL_SetWindowTitle(win, title);
            frameCount = 0;
            fpsTimer = now;
        }

        Uint32 frameTime = SDL_GetTicks() - now;
        if (frameTime < 30) SDL_Delay(30 - frameTime);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
