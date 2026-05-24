#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "gpu_tracer.hpp"

// ─── Metal shader source ─────────────────────────────────────────────────────
static NSString* shader_src = @R"(
#include <metal_stdlib>
using namespace metal;

struct Sphere {
    packed_float3 center;
    float radius;
    packed_float3 color;
    float reflect;
    float ior;
    float roughness;
};

struct AreaLight {
    packed_float3 pos;
    packed_float3 u;
    packed_float3 v;
    packed_float3 emission;
};

// ─── Deterministic RNG ──────────────────────────────────────────────────────
float rng(uint x, uint y, uint bounce, uint sample) {
    uint h = x * 374761393u + y * 668265263u + bounce * 1274126177u + sample * 1013904223u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h & 0xFFFFFFu) / 16777216.0;
}

float2 rng2(uint x, uint y, uint bounce, uint s) {
    return float2(rng(x, y, bounce, s), rng(x, y, bounce, s + 1000));
}

// ─── Concentric disk sampling ────────────────────────────────────────────────
float2 concentric_disk(float2 u) {
    float2 o = 2.0 * u - 1.0;
    if (o.x == 0 && o.y == 0) return float2(0, 0);
    float r, theta;
    if (fabs(o.x) > fabs(o.y)) {
        r = o.x;
        theta = M_PI_F * 0.25 * (o.y / o.x);
    } else {
        r = o.y;
        theta = M_PI_F * 0.5 - M_PI_F * 0.25 * (o.x / o.y);
    }
    return r * float2(cos(theta), sin(theta));
}

// ─── Sphere intersection ────────────────────────────────────────────────────
float hit_sphere(Sphere s, float3 ro, float3 rd) {
    float3 oc = ro - s.center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - s.radius * s.radius;
    float disc = b*b - c;
    if (disc < 0) return -1;
    float sd = sqrt(disc);
    float t = -b - sd;
    if (t < 0.001) t = -b + sd;
    return t >= 0.001 ? t : -1;
}

// ─── Rectangle light intersection ────────────────────────────────────────────
float hit_light(AreaLight l, float3 ro, float3 rd) {
    float3 nrm = normalize(cross(l.u, l.v));
    float denom = dot(rd, nrm);
    if (fabs(denom) < 1e-8) return -1;
    float t = dot(l.pos - ro, nrm) / denom;
    if (t < 0.001) return -1;
    float3 p = ro + rd * t;
    float3 d = p - l.pos;
    float du = dot(d, l.u) / dot(l.u, l.u);
    float dv = dot(d, l.v) / dot(l.v, l.v);
    return (du >= -1 && du <= 1 && dv >= -1 && dv <= 1) ? t : -1;
}

// ─── Fresnel ─────────────────────────────────────────────────────────────────
float fresnel_schlick(float cos_theta, float eta) {
    float r0 = (1.0 - eta) / (1.0 + eta);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// ─── Checkerboard ────────────────────────────────────────────────────────────
float3 checker(float3 p, float3 c0, float3 c1) {
    float u = p.x + p.z * 0.5;
    float v = p.z - p.x * 0.5;
    int iu = (int)floor(u * 3.0);
    int iv = (int)floor(v * 3.0);
    return ((iu + iv) & 1) ? c0 : c1;
}

// ─── ACES tone mapping ───────────────────────────────────────────────────────
float3 aces(float3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ─── Build orthonormal basis from a normal ───────────────────────────────────
void basis_from_normal(float3 n, thread float3& u, thread float3& v) {
    if (fabs(n.y) < 0.9) {
        u = normalize(cross(n, float3(0, 1, 0)));
        v = cross(n, u);
    } else {
        u = normalize(cross(n, float3(1, 0, 0)));
        v = cross(n, u);
    }
}

// ─── Main kernel ─────────────────────────────────────────────────────────────
kernel void raytrace(
    device uint* pixels [[buffer(0)]],
    constant Sphere* spheres [[buffer(1)]],
    constant uint& num_spheres [[buffer(2)]],
    constant float3& light_dir [[buffer(3)]],
    constant float3& cam_pos [[buffer(4)]],
    constant uint& img_width [[buffer(5)]],
    constant uint& img_height [[buffer(6)]],
    constant AreaLight& area_light [[buffer(7)]],
    uint2 tid [[thread_position_in_grid]])
{
    int x = tid.x;
    int y = tid.y;
    if (x >= img_width || y >= img_height) return;

    float aspect = (float)img_width / (float)img_height;
    float fov = 0.7;
    const int SAMPLES  = 4;
    const int BOUNCES  = 8;
    const int AREA_SMP = 24;
    const float APERTURE  = 0.018;
    const float FOCAL_DIST = 6.0;

    float3 col = float3(0, 0, 0);

    for (int sy = 0; sy < SAMPLES; sy++) {
        for (int sx = 0; sx < SAMPLES; sx++) {
            int s = sy * SAMPLES + sx;

            float u = (x + (sx + 0.5) / (float)SAMPLES) / img_width;
            float v = (y + (sy + 0.5) / (float)SAMPLES) / img_height;
            float px = (2.0 * u - 1.0) * aspect * fov;
            float py = (1.0 - 2.0 * v) * fov;
            float3 rd = normalize(float3(px, py, 1.0));

            // Depth of field
            float3 ro = cam_pos;
            if (APERTURE > 0) {
                float2 lens_rv = rng2(x, y, s, 9999);
                float2 lens_uv = concentric_disk(lens_rv) * APERTURE;
                float3 focal_pt = cam_pos + rd * FOCAL_DIST;
                ro += float3(lens_uv, 0);
                rd = normalize(focal_pt - ro);
            }

            float3 throughput = float3(1, 1, 1);

            for (int bounce = 0; bounce <= BOUNCES; bounce++) {
                // ── Sphere intersection ────────────────────────────────
                float t = 1e10;
                int idx = -1;
                for (int i = 0; i < num_spheres; i++) {
                    float h = hit_sphere(spheres[i], ro, rd);
                    if (h > 0 && h < t) { t = h; idx = i; }
                }

                // ── Area light intersection ────────────────────────────
                float lt = hit_light(area_light, ro, rd);
                if (lt > 0 && lt < t) {
                    col += throughput * area_light.emission;
                    break;
                }

                // ── Sky ────────────────────────────────────────────────
                if (idx < 0) {
                    float sky = 0.5 + 0.5 * rd.y;
                    col += throughput * float3(0.3, 0.5, 0.9) * sky * 0.6;
                    break;
                }

                float3 p = ro + rd * t;
                float3 n = normalize(p - spheres[idx].center);
                float3 mat_color = spheres[idx].color;
                float reflectivity = spheres[idx].reflect;
                float ior = spheres[idx].ior;
                float roughness = spheres[idx].roughness;

                // ── Glass ──────────────────────────────────────────────
                if (ior > 0.01) {
                    bool entering = dot(n, rd) < 0;
                    float eta = entering ? (1.0 / ior) : ior;
                    if (!entering) n = -n;

                    float cos_theta = -dot(n, rd);
                    float k = 1.0 - eta * eta * (1.0 - cos_theta * cos_theta);

                    if (k < 0) {
                        rd = reflect(rd, n);
                        ro = p + n * 0.002;
                        continue;
                    }

                    float3 refr_dir = eta * rd + (eta * cos_theta - sqrt(k)) * n;
                    float fr = fresnel_schlick(cos_theta, eta);

                    if (rng(x, y, bounce, s) < fr) {
                        rd = reflect(rd, n);
                        ro = p + n * 0.002;
                    } else {
                        float3 entry = p + refr_dir * 0.002;
                        float3 oc2 = entry - spheres[idx].center;
                        float b2 = dot(oc2, refr_dir);
                        float c2 = dot(oc2, oc2) - spheres[idx].radius * spheres[idx].radius;
                        float disc2 = b2 * b2 - c2;
                        if (disc2 > 0) {
                            float t_exit = -b2 + sqrt(disc2);
                            float3 exit_pt = entry + refr_dir * t_exit;
                            float3 exit_n = normalize(exit_pt - spheres[idx].center);
                            if (!entering) exit_n = -exit_n;

                            float3 beer = exp(-(1.0 - mat_color) * 0.3 * t_exit);
                            throughput *= beer;

                            float eta2 = entering ? ior : (1.0 / ior);
                            float ct2 = -dot(exit_n, refr_dir);
                            float k2 = 1.0 - eta2 * eta2 * (1.0 - ct2 * ct2);
                            if (k2 > 0) {
                                float3 exit_dir = eta2 * refr_dir + (eta2 * ct2 - sqrt(k2)) * -exit_n;
                                ro = exit_pt + exit_dir * 0.002;
                                rd = normalize(exit_dir);
                            } else {
                                rd = reflect(refr_dir, exit_n);
                                ro = exit_pt + exit_n * 0.002;
                            }
                        } else {
                            rd = reflect(rd, n);
                            ro = p + n * 0.002;
                        }
                    }
                    continue;
                }

                // ── Opaque material shading ────────────────────────────
                float3 surf_color = mat_color;
                if (idx == 0) {
                    surf_color = checker(p, float3(0.7, 0.7, 0.7), float3(0.15, 0.15, 0.2));
                }

                col += throughput * surf_color * 0.05;

                // ── Area light soft shadows ────────────────────────────
                float3 lp = area_light.pos;
                float sh_accum = 0;

                for (int sj = 0; sj < AREA_SMP; sj++) {
                    float2 rv = rng2(x, y, bounce * 100 + s, sj);
                    float3 light_pt = area_light.pos
                        + area_light.u * (rv.x * 2.0 - 1.0)
                        + area_light.v * (rv.y * 2.0 - 1.0);
                    float3 to_light = light_pt - p;
                    float light_dist = length(to_light);
                    float3 ldir = to_light / light_dist;

                    float3 shad_ro = p + n * 0.003;
                    float st = light_dist;
                    int sidx = -1;
                    for (int i = 0; i < num_spheres; i++) {
                        float h = hit_sphere(spheres[i], shad_ro, ldir);
                        if (h > 0 && h < st) { st = h; sidx = i; }
                    }

                    if (sidx < 0) {
                        float cos_light = max(dot(n, ldir), 0.0);
                        float cos_light_n = max(dot(-ldir, normalize(cross(area_light.u, area_light.v))), 0.0);
                        sh_accum += cos_light * cos_light_n / light_dist;
                    }
                }

                float area_scale = 4.0 * length(area_light.u) * length(area_light.v) / (float)AREA_SMP;
                float3 area_contrib = area_light.emission * sh_accum * area_scale;
                col += throughput * surf_color * area_contrib;

                // ── Directional light ──────────────────────────────────
                float3 ld = normalize(light_dir);
                float diff = max(dot(n, ld), 0.0);
                if (diff > 0) {
                    float3 shad_ro = p + n * 0.003;
                    float st = 1e10;
                    int sidx = -1;
                    for (int i = 0; i < num_spheres; i++) {
                        float h = hit_sphere(spheres[i], shad_ro, ld);
                        if (h > 0 && h < st) { st = h; sidx = i; }
                    }
                    float shadow = (sidx < 0) ? 1.0 : 0.2;
                    col += throughput * surf_color * diff * 0.3 * shadow;

                    float3 h_vec = normalize(ld - rd);
                    float spec = pow(max(dot(n, h_vec), 0.0), 64);
                    col += throughput * float3(1, 1, 1) * spec * 0.2 * shadow;
                }

                // ── Glossy reflection ──────────────────────────────────
                if (bounce < BOUNCES && reflectivity > 0) {
                    throughput *= reflectivity;
                    float3 refl = reflect(rd, n);

                    if (roughness > 0.001) {
                        float3 ru, rv;
                        basis_from_normal(refl, ru, rv);
                        float2 disk = concentric_disk(rng2(x, y, bounce * 50 + s, 555)) * roughness;
                        refl = normalize(refl + ru * disk.x + rv * disk.y);
                    }

                    rd = refl;
                    ro = p + n * 0.002;
                } else {
                    break;
                }
            }
        }
    }

    col = col / (float)(SAMPLES * SAMPLES);
    col = aces(col * 1.3);
    col = clamp(col, 0.0, 1.0);

    uint r = (uint)(col.x * 255);
    uint g = (uint)(col.y * 255);
    uint b = (uint)(col.z * 255);
    pixels[y * img_width + x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
}
)";

// ─── GPU tracer ObjC implementation ──────────────────────────────────────────
@interface GpuTracerImpl : NSObject
@property(strong) id<MTLDevice> device;
@property(strong) id<MTLCommandQueue> queue;
@property(nonatomic, strong) id<MTLComputePipelineState> pipeline;
@property(strong) id<MTLBuffer> pixelBuf;
@property(strong) id<MTLBuffer> spheresBuf;
@property(strong) id<MTLBuffer> numSpheresBuf;
@property(strong) id<MTLBuffer> lightDirBuf;
@property(strong) id<MTLBuffer> camPosBuf;
@property(strong) id<MTLBuffer> widthBuf;
@property(strong) id<MTLBuffer> heightBuf;
@property(strong) id<MTLBuffer> areaLightBuf;
- (instancetype)initWithWidth:(int)width height:(int)height scene:(const GpuScene&)scene;
- (void)render;
- (const uint32_t*)pixels;
@end

@implementation GpuTracerImpl
- (instancetype)initWithWidth:(int)width height:(int)height scene:(const GpuScene&)scene {
    if ((self = [super init])) {
        _device = MTLCreateSystemDefaultDevice();
        _queue = [_device newCommandQueue];

        NSError* err = nil;
        id<MTLLibrary> lib = [_device newLibraryWithSource:shader_src options:nil error:&err];
        if (!lib) { NSLog(@"Shader error: %@", err); abort(); }
        id<MTLFunction> fn = [lib newFunctionWithName:@"raytrace"];
        _pipeline = [_device newComputePipelineStateWithFunction:fn error:&err];
        if (!_pipeline) { NSLog(@"Pipeline error: %@", err); abort(); }

        _pixelBuf = [_device newBufferWithLength:width * height * 4
                                        options:MTLResourceStorageModeShared];

        GpuSphere gs[8];
        for (int i = 0; i < 8; i++) gs[i] = scene.spheres[i];
        _spheresBuf = [_device newBufferWithBytes:gs length:sizeof(gs)
                                        options:MTLResourceStorageModeShared];

        uint ns = 8;
        _numSpheresBuf = [_device newBufferWithBytes:&ns length:4
                                            options:MTLResourceStorageModeShared];

        float len = sqrtf(scene.light_dir[0] * scene.light_dir[0] +
                          scene.light_dir[1] * scene.light_dir[1] +
                          scene.light_dir[2] * scene.light_dir[2]);
        float lx = scene.light_dir[0] / len;
        float ly = scene.light_dir[1] / len;
        float lz = scene.light_dir[2] / len;
        float ld[4] = {lx, ly, lz, 0};
        _lightDirBuf = [_device newBufferWithBytes:ld length:16
                                          options:MTLResourceStorageModeShared];

        float cam[4] = {0, 0.7, -4, 0};
        _camPosBuf = [_device newBufferWithBytes:cam length:16
                                        options:MTLResourceStorageModeShared];

        _widthBuf  = [_device newBufferWithBytes:&width length:4
                                        options:MTLResourceStorageModeShared];
        _heightBuf = [_device newBufferWithBytes:&height length:4
                                        options:MTLResourceStorageModeShared];

        float al_data[12];
        for (int i = 0; i < 3; i++) {
            al_data[i]      = scene.area_light_pos[i];
            al_data[3 + i]  = scene.area_light_u[i];
            al_data[6 + i]  = scene.area_light_v[i];
            al_data[9 + i]  = scene.area_light_emission[i];
        }
        _areaLightBuf = [_device newBufferWithBytes:al_data length:48
                                           options:MTLResourceStorageModeShared];
    }
    return self;
}

- (void)render {
    id<MTLCommandBuffer> cb = [_queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:_pipeline];
    [enc setBuffer:_pixelBuf offset:0 atIndex:0];
    [enc setBuffer:_spheresBuf offset:0 atIndex:1];
    [enc setBuffer:_numSpheresBuf offset:0 atIndex:2];
    [enc setBuffer:_lightDirBuf offset:0 atIndex:3];
    [enc setBuffer:_camPosBuf offset:0 atIndex:4];
    [enc setBuffer:_widthBuf offset:0 atIndex:5];
    [enc setBuffer:_heightBuf offset:0 atIndex:6];
    [enc setBuffer:_areaLightBuf offset:0 atIndex:7];

    MTLSize grid = { (NSUInteger)*(int*)[_widthBuf contents],
                     (NSUInteger)*(int*)[_heightBuf contents], 1 };
    MTLSize tg = { 8, 8, 1 };
    [enc dispatchThreads:grid threadsPerThreadgroup:tg];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
}

- (const uint32_t*)pixels {
    return (const uint32_t*)[_pixelBuf contents];
}
@end

// ─── C++ wrapper ────────────────────────────────────────────────────────────
GpuTracer::GpuTracer(int width, int height, const GpuScene& scene)
    : width_(width), height_(height)
{
    impl_ = (__bridge_retained void*)[[GpuTracerImpl alloc] initWithWidth:width height:height scene:scene];
}

GpuTracer::~GpuTracer() {
    (void)(__bridge_transfer id)impl_;
}

void GpuTracer::render() {
    [(__bridge GpuTracerImpl*)impl_ render];
}

const uint32_t* GpuTracer::pixels() const {
    return [(__bridge GpuTracerImpl*)impl_ pixels];
}
