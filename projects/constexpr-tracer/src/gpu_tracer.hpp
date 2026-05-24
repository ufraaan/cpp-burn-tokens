#pragma once
#include <cstdint>

struct GpuSphere {
    float center[3];
    float radius;
    float color[3];
    float reflect;
    float ior;
    float roughness;
};

struct GpuScene {
    GpuSphere spheres[8];
    float light_dir[3];
    float area_light_pos[3];
    float area_light_u[3];
    float area_light_v[3];
    float area_light_emission[3];
};

class GpuTracer {
    int width_, height_;
    void* impl_;

public:
    GpuTracer(int width, int height, const GpuScene& scene);
    ~GpuTracer();
    void render();
    const uint32_t* pixels() const;
};
