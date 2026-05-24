#pragma once
#include <array>
#include <cstdint>
#include "vec3.hpp"

constexpr int MAX_BOUNCES = 3;

// ─── Material ────────────────────────────────────────────────────────────────
struct Material {
    Vec3 color;
    float reflect;
};

// ─── Sphere ──────────────────────────────────────────────────────────────────
struct Sphere {
    Vec3 center;
    float radius;
    Material mat;

    constexpr float hit(Vec3 ro, Vec3 rd) const {
        Vec3 oc = ro - center;
        float b = oc.dot(rd);
        float c = oc.dot(oc) - radius * radius;
        float disc = b*b - c;
        if (disc < 0) return -1;
        float sd = csqrt(disc);
        float t = -b - sd;
        if (t < 0.001f) t = -b + sd;
        return t >= 0.001f ? t : -1;
    }
};

// ─── Scene ───────────────────────────────────────────────────────────────────
struct Scene {
    std::array<Sphere, 5> spheres;
    Vec3 light_dir;

    constexpr Scene()
        : spheres{{
            {{0, -1001, 0},      1000, {{0.5f, 0.5f, 0.5f}, 0.0f}},
            {{-1.0f, 0.0f, 2.0f}, 0.5f, {{1.0f, 0.2f, 0.2f}, 0.0f}},
            {{ 1.2f, 0.1f, 1.5f}, 0.7f, {{0.2f, 0.2f, 1.0f}, 0.3f}},
            {{ 0.0f,-0.1f, 4.0f}, 0.4f, {{0.2f, 1.0f, 0.2f}, 0.0f}},
            {{-0.3f, 0.5f, 1.5f}, 0.4f, {{1.0f, 1.0f, 1.0f}, 0.7f}},
          }},
          light_dir(Vec3{0.8f, 1.0f, 0.6f}.norm())
    {}

    constexpr int closest(Vec3 ro, Vec3 rd, float& t, Material& mat, Vec3& n) const {
        t = 1e10f;
        int idx = -1;
        for (int i = 0; i < (int)spheres.size(); i++) {
            float h = spheres[i].hit(ro, rd);
            if (h > 0 && h < t) { t = h; idx = i; }
        }
        if (idx >= 0) {
            Vec3 p = ro + rd * t;
            n = (p - spheres[idx].center).norm();
            mat = spheres[idx].mat;
        }
        return idx;
    }

    constexpr Vec3 trace(Vec3 ro, Vec3 rd, int depth) const {
        float t = 0;
        Material mat{};
        Vec3 n{};
        int idx = closest(ro, rd, t, mat, n);

        // Miss → sky gradient
        if (idx < 0) {
            float sky = 0.5f + 0.5f * rd.y;
            return Vec3{0.5f, 0.7f, 1.0f} * sky;
        }

        Vec3 p = ro + rd * t;
        Vec3 col = mat.color * 0.1f;  // ambient

        // Direct light
        float diff = n.dot(light_dir);
        if (diff > 0) {
            // Shadow ray
            Vec3 shad_ro = p + n * 0.001f;
            float st = 0;
            Material sm{};
            Vec3 sn{};
            int sidx = closest(shad_ro, light_dir, st, sm, sn);
            float shadow = (sidx < 0) ? 1.0f : 0.25f;

            col += mat.color * diff * 0.6f * shadow;

            // Specular (Blinn-Phong)
            Vec3 h = (light_dir - rd).norm();
            float spec = cpow(clamp(n.dot(h), 0.0f, 1.0f), 32);
            col += Vec3{1, 1, 1} * spec * 0.4f * shadow;
        }

        // Reflection
        if (mat.reflect > 0 && depth < MAX_BOUNCES) {
            Vec3 refl = rd - n * (2.0f * rd.dot(n));
            Vec3 rc = trace(p + n * 0.001f, refl.norm(), depth + 1);
            col = col * (1.0f - mat.reflect) + rc * mat.reflect;
        }

        return clamp(col, 0.0f, 1.0f);
    }
};

// ─── Compile-time renderer ───────────────────────────────────────────────────
template<int W, int H>
struct Image {
    std::array<uint32_t, W * H> pixels{};
};

template<int W, int H>
constexpr Image<W, H> render() {
    Image<W, H> img;
    Scene scene;
    Vec3 cam_pos{0, 0.7f, -4};
    float fov = 0.7f;
    float aspect = (float)W / H;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float px = (2.0f * (x + 0.5f) / W - 1.0f) * aspect * fov;
            float py = (1.0f - 2.0f * (y + 0.5f) / H) * fov;
            Vec3 rd = Vec3{px, py, 1.0f}.norm();
            Vec3 col = scene.trace(cam_pos, rd, 0);
            uint8_t r = (uint8_t)(clamp(col.x, 0.0f, 1.0f) * 255);
            uint8_t g = (uint8_t)(clamp(col.y, 0.0f, 1.0f) * 255);
            uint8_t b = (uint8_t)(clamp(col.z, 0.0f, 1.0f) * 255);
            img.pixels[y * W + x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return img;
}
