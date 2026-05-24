#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "gpu_tracer.hpp"

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

// ─── Interleaved gradient noise (blue-noise spectrum, entirely procedural) ──
float ign(float2 p) {
    float3 s = float3(p, 0.0);
    s = fract(s * 0.1031);
    s += dot(s, s.yzx + 33.33);
    return fract((s.x + s.y) * s.z);
}
float2 ign2(float2 p) {
    return float2(ign(p), ign(p + float2(100.f, 100.f)));
}

// ─── Halton low-discrepancy sequence ────────────────────────────────────────
float halton(int idx, int base) {
    float r = 0.f;
    float f = 1.f / (float)base;
    int i = idx;
    while (i > 0) {
        r += (float)(i % base) * f;
        i /= base;
        f /= (float)base;
    }
    return r;
}
float2 halton2(int idx, float scramble) {
    int s = (int)(scramble * 10000.f);
    return float2(halton(idx + s, 2), halton(idx + s, 3));
}

// ─── Concentric disk sampling ────────────────────────────────────────────────
float2 concentric_disk(float2 u) {
    float2 o = 2.f * u - 1.f;
    if (o.x == 0 && o.y == 0) return float2(0, 0);
    float r, theta;
    if (fabs(o.x) > fabs(o.y)) {
        r = o.x;
        theta = M_PI_F * 0.25f * (o.y / o.x);
    } else {
        r = o.y;
        theta = M_PI_F * 0.5f - M_PI_F * 0.25f * (o.x / o.y);
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
    if (t < 0.001f) t = -b + sd;
    return t >= 0.001f ? t : -1;
}

// ─── Rectangle light intersection ────────────────────────────────────────────
float hit_light(AreaLight l, float3 ro, float3 rd) {
    float3 nrm = normalize(cross(l.u, l.v));
    float denom = dot(rd, nrm);
    if (fabs(denom) < 1e-8f) return -1;
    float t = dot(l.pos - ro, nrm) / denom;
    if (t < 0.001f) return -1;
    float3 p = ro + rd * t;
    float3 d = p - l.pos;
    float du = dot(d, l.u) / dot(l.u, l.u);
    float dv = dot(d, l.v) / dot(l.v, l.v);
    return (du >= -1 && du <= 1 && dv >= -1 && dv <= 1) ? t : -1;
}

// ─── Fresnel ─────────────────────────────────────────────────────────────────
float fresnel_schlick(float cos_theta, float eta) {
    float r0 = (1.f - eta) / (1.f + eta);
    r0 = r0 * r0;
    return r0 + (1.f - r0) * pow(1.f - cos_theta, 5.f);
}

// ─── Checkerboard ────────────────────────────────────────────────────────────
float3 checker(float3 p, float3 c0, float3 c1) {
    float u = p.x + p.z * 0.5f;
    float v = p.z - p.x * 0.5f;
    int iu = (int)floor(u * 3.f);
    int iv = (int)floor(v * 3.f);
    return ((iu + iv) & 1) ? c0 : c1;
}

// ─── ACES tone mapping ───────────────────────────────────────────────────────
float3 aces(float3 x) {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.f, 1.f);
}

// ─── Build orthonormal basis from a normal ───────────────────────────────────
void basis_from_normal(float3 n, thread float3& ru, thread float3& rv) {
    if (fabs(n.y) < 0.9f) {
        ru = normalize(cross(n, float3(0, 1, 0)));
        rv = cross(n, ru);
    } else {
        ru = normalize(cross(n, float3(1, 0, 0)));
        rv = cross(n, ru);
    }
}

// Constants
constant int SAMPLES  = 6;   // 6×6 = 36 AA samples per pixel
constant int BOUNCES  = 8;
constant int AREA_SMP = 24;
constant float APERTURE  = 0.018f;
constant float FOCAL_DIST = 6.f;

// ─── Sub-path tracer: follows a ray for remaining bounces ──────────────────
// Uses the hash lottery for nested glass (no further splitting).
float3 trace_path(constant Sphere* spheres, constant uint& ns,
                  constant float3& ldir, constant AreaLight& al,
                  float3 ro, float3 rd, float3 throughput, int start_bounce,
                  int px, int py, int sample, float scramble)
{
    float3 col = 0.f;

    for (int bounce = start_bounce; bounce <= BOUNCES; bounce++) {
        float t = 1e10f;
        int idx = -1;
        for (int i = 0; i < ns; i++) {
            float h = hit_sphere(spheres[i], ro, rd);
            if (h > 0 && h < t) { t = h; idx = i; }
        }

        float lt = hit_light(al, ro, rd);
        if (lt > 0 && lt < t) {
            col += throughput * al.emission;
            break;
        }

        if (idx < 0) {
            float sky = 0.5f + 0.5f * rd.y;
            col += throughput * float3(0.3f, 0.5f, 0.9f) * sky * 0.6f;
            break;
        }

        float3 p = ro + rd * t;
        float3 n = normalize(p - spheres[idx].center);
        float3 mat_color = spheres[idx].color;
        float reflectivity = spheres[idx].reflect;
        float ior = spheres[idx].ior;
        float roughness = spheres[idx].roughness;

        // ── Glass (hash lottery only — no further splitting) ─────────
        if (ior > 0.01f) {
            bool entering = dot(n, rd) < 0;
            float eta = entering ? (1.f / ior) : ior;
            if (!entering) n = -n;

            float cos_theta = -dot(n, rd);
            float k = 1.f - eta * eta * (1.f - cos_theta * cos_theta);

            if (k < 0) {
                rd = reflect(rd, n);
                ro = p + n * 0.002f;
                continue;
            }

            float3 refr_dir = eta * rd + (eta * cos_theta - sqrt(k)) * n;
            float fr = fresnel_schlick(cos_theta, eta);

            float2 rv = ign2(float2((float)(px * BOUNCES * SAMPLES * 100 + bounce * 100 + sample),
                                     (float)(py * 37 + bounce * 7)));
            if (rv.x < fr) {
                rd = reflect(rd, n);
                ro = p + n * 0.002f;
            } else {
                float3 entry = p + refr_dir * 0.002f;
                float3 oc2 = entry - spheres[idx].center;
                float b2 = dot(oc2, refr_dir);
                float c2 = dot(oc2, oc2) - spheres[idx].radius * spheres[idx].radius;
                float disc2 = b2 * b2 - c2;
                if (disc2 > 0) {
                    float t_exit = -b2 + sqrt(disc2);
                    float3 exit_pt = entry + refr_dir * t_exit;
                    float3 exit_n = normalize(exit_pt - spheres[idx].center);
                    if (!entering) exit_n = -exit_n;

                    float3 beer = exp(-(1.f - mat_color) * 0.3f * t_exit);
                    throughput *= beer;

                    float eta2 = entering ? ior : (1.f / ior);
                    float ct2 = -dot(exit_n, refr_dir);
                    float k2 = 1.f - eta2 * eta2 * (1.f - ct2 * ct2);
                    if (k2 > 0) {
                        float3 exit_dir = eta2 * refr_dir + (eta2 * ct2 - sqrt(k2)) * -exit_n;
                        ro = exit_pt + exit_dir * 0.002f;
                        rd = normalize(exit_dir);
                    } else {
                        rd = reflect(refr_dir, exit_n);
                        ro = exit_pt + exit_n * 0.002f;
                    }
                } else {
                    rd = reflect(rd, n);
                    ro = p + n * 0.002f;
                }
            }
            continue;
        }

        // ── Opaque material shading ───────────────────────────────────
        float3 surf_color = mat_color;
        if (idx == 0) {
            surf_color = checker(p, float3(0.7f, 0.7f, 0.7f), float3(0.15f, 0.15f, 0.2f));
        }

        col += throughput * surf_color * 0.05f;

        // ── Area light with Halton low-discrepancy shadow samples ────
        float sh_accum = 0.f;
        for (int sj = 0; sj < AREA_SMP; sj++) {
            int hidx = px * (SAMPLES * SAMPLES * BOUNCES)
                     + py * (SAMPLES * BOUNCES)
                     + sample * BOUNCES
                     + bounce * AREA_SMP + sj;
            float2 hv = halton2(hidx, scramble);

            float3 light_pt = al.pos + al.u * (hv.x * 2.f - 1.f)
                                    + al.v * (hv.y * 2.f - 1.f);
            float3 to_light = light_pt - p;
            float light_dist = length(to_light);
            float3 ldir_s = to_light / light_dist;

            float3 shad_ro = p + n * 0.003f;
            float st = light_dist;
            int sidx = -1;
            for (int i = 0; i < ns; i++) {
                float h = hit_sphere(spheres[i], shad_ro, ldir_s);
                if (h > 0 && h < st) { st = h; sidx = i; }
            }

            if (sidx < 0) {
                float cos_light = max(dot(n, ldir_s), 0.f);
                float cos_light_n = max(dot(-ldir_s, normalize(cross(al.u, al.v))), 0.f);
                sh_accum += (cos_light * cos_light_n) / (light_dist * light_dist);
            }
        }

        float area_scale = 4.f * length(al.u) * length(al.v) / (float)AREA_SMP;
        col += throughput * surf_color * al.emission * sh_accum * area_scale;

        // ── Directional light ─────────────────────────────────────────
        float diff = max(dot(n, ldir), 0.f);
        if (diff > 0) {
            float3 shad_ro = p + n * 0.003f;
            float st = 1e10f;
            int sidx = -1;
            for (int i = 0; i < ns; i++) {
                float h = hit_sphere(spheres[i], shad_ro, ldir);
                if (h > 0 && h < st) { st = h; sidx = i; }
            }
            float shadow = (sidx < 0) ? 1.f : 0.2f;
            col += throughput * surf_color * diff * 0.3f * shadow;

            float3 h_vec = normalize(ldir - rd);
            float spec = pow(max(dot(n, h_vec), 0.f), 64);
            col += throughput * float3(1, 1, 1) * spec * 0.2f * shadow;
        }

        // ── Glossy reflection ─────────────────────────────────────────
        if (bounce < BOUNCES && reflectivity > 0) {
            throughput *= reflectivity;
            float3 refl = reflect(rd, n);

            if (roughness > 0.001f) {
                float3 ru, rv;
                basis_from_normal(refl, ru, rv);
                float2 rv2 = ign2(float2((float)(px * 500 + sample * 50 + bounce * 30),
                                          (float)(py * 200 + sample * 7)));
                float2 disk = concentric_disk(rv2) * roughness;
                refl = normalize(refl + ru * disk.x + rv * disk.y);
            }

            rd = refl;
            ro = p + n * 0.002f;
        } else {
            break;
        }
    }
    return col;
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
    float fov = 0.7f;

    // Per-pixel Halton scramble for decorrelation
    float scramble = ign(float2((float)x, (float)y));

    float3 col = 0.f;

    for (int sy = 0; sy < SAMPLES; sy++) {
        for (int sx = 0; sx < SAMPLES; sx++) {
            int s = sy * SAMPLES + sx;

            float u = (x + (sx + 0.5f) / (float)SAMPLES) / img_width;
            float v = (y + (sy + 0.5f) / (float)SAMPLES) / img_height;
            float px = (2.f * u - 1.f) * aspect * fov;
            float py = (1.f - 2.f * v) * fov;
            float3 rd = normalize(float3(px, py, 1.f));

            // Depth of field
            float3 ro = cam_pos;
            if (APERTURE > 0) {
                float2 rv = ign2(float2((float)(x * 100 + s * 7), (float)(y * 100 + s * 3)));
                float2 lens_uv = concentric_disk(rv) * APERTURE;
                float3 focal_pt = cam_pos + rd * FOCAL_DIST;
                ro += float3(lens_uv, 0);
                rd = normalize(focal_pt - ro);
            }

            float3 throughput = float3(1, 1, 1);

            // ── Single bounce (primary hit) ──────────────────────────
            float t = 1e10f;
            int idx = -1;
            for (int i = 0; i < num_spheres; i++) {
                float h = hit_sphere(spheres[i], ro, rd);
                if (h > 0 && h < t) { t = h; idx = i; }
            }

            float lt = hit_light(area_light, ro, rd);
            if (lt > 0 && lt < t) {
                col += throughput * area_light.emission;
                continue;
            }

            if (idx < 0) {
                float sky = 0.5f + 0.5f * rd.y;
                col += throughput * float3(0.3f, 0.5f, 0.9f) * sky * 0.6f;
                continue;
            }

            float3 p = ro + rd * t;
            float3 n = normalize(p - spheres[idx].center);
            float3 mat_color = spheres[idx].color;
            float reflectivity = spheres[idx].reflect;
            float ior = spheres[idx].ior;
            float roughness = spheres[idx].roughness;

            // ── Glass: Fresnel-weighted path splitting ────────────────
            if (ior > 0.01f) {
                bool entering = dot(n, rd) < 0;
                float eta = entering ? (1.f / ior) : ior;
                if (!entering) n = -n;

                float cos_theta = -dot(n, rd);
                float k = 1.f - eta * eta * (1.f - cos_theta * cos_theta);

                if (k > 0) {
                    float3 refr_dir = eta * rd + (eta * cos_theta - sqrt(k)) * n;
                    float fr = fresnel_schlick(cos_theta, eta);

                    // Reflection path
                    float3 refl_rd = reflect(rd, n);
                    float3 refl_col = trace_path(spheres, num_spheres, light_dir,
                        area_light, p + n * 0.002f, refl_rd, throughput * fr,
                        1, x, y, s, scramble);

                    // Refraction path
                    float3 entry = p + refr_dir * 0.002f;
                    float3 oc2 = entry - spheres[idx].center;
                    float b2 = dot(oc2, refr_dir);
                    float c2 = dot(oc2, oc2) - spheres[idx].radius * spheres[idx].radius;
                    float disc2 = b2 * b2 - c2;

                    float3 refr_col = 0.f;
                    if (disc2 > 0) {
                        float t_exit = -b2 + sqrt(disc2);
                        float3 exit_pt = entry + refr_dir * t_exit;
                        float3 exit_n = normalize(exit_pt - spheres[idx].center);
                        if (!entering) exit_n = -exit_n;

                        float3 beer = exp(-(1.f - mat_color) * 0.3f * t_exit);

                        float eta2 = entering ? ior : (1.f / ior);
                        float ct2 = -dot(exit_n, refr_dir);
                        float k2 = 1.f - eta2 * eta2 * (1.f - ct2 * ct2);
                        if (k2 > 0) {
                            float3 exit_dir = eta2 * refr_dir + (eta2 * ct2 - sqrt(k2)) * -exit_n;
                            refr_col = trace_path(spheres, num_spheres, light_dir,
                                area_light, exit_pt + exit_dir * 0.002f,
                                normalize(exit_dir), throughput * (1.f - fr) * beer,
                                1, x, y, s, scramble);
                        } else {
                            float3 exit_refl = reflect(refr_dir, exit_n);
                            refr_col = trace_path(spheres, num_spheres, light_dir,
                                area_light, exit_pt + exit_n * 0.002f,
                                exit_refl, throughput * (1.f - fr) * beer,
                                1, x, y, s, scramble);
                        }
                    }

                    col += refl_col + refr_col;
                } else {
                    col += trace_path(spheres, num_spheres, light_dir, area_light,
                        p + n * 0.002f, reflect(rd, n), throughput, 1,
                        x, y, s, scramble);
                }
                continue;
            }

            // ── Opaque primary hit ─────────────────────────────────────
            float3 surf_color = mat_color;
            if (idx == 0) {
                surf_color = checker(p, float3(0.7f, 0.7f, 0.7f), float3(0.15f, 0.15f, 0.2f));
            }

            col += throughput * surf_color * 0.05f;

            float sh_accum = 0.f;
            for (int sj = 0; sj < AREA_SMP; sj++) {
                int hidx = x * (SAMPLES * SAMPLES * BOUNCES)
                         + y * (SAMPLES * BOUNCES)
                         + s * BOUNCES + sj;
                float2 hv = halton2(hidx, scramble);

                float3 light_pt = area_light.pos
                    + area_light.u * (hv.x * 2.f - 1.f)
                    + area_light.v * (hv.y * 2.f - 1.f);
                float3 to_light = light_pt - p;
                float light_dist = length(to_light);
                float3 ldir_s = to_light / light_dist;

                float3 shad_ro = p + n * 0.003f;
                float st = light_dist;
                int sidx = -1;
                for (int i = 0; i < num_spheres; i++) {
                    float h = hit_sphere(spheres[i], shad_ro, ldir_s);
                    if (h > 0 && h < st) { st = h; sidx = i; }
                }

                if (sidx < 0) {
                    float cos_light = max(dot(n, ldir_s), 0.f);
                    float cos_light_n = max(dot(-ldir_s, normalize(cross(area_light.u, area_light.v))), 0.f);
                    sh_accum += (cos_light * cos_light_n) / (light_dist * light_dist);
                }
            }

            float area_scale = 4.f * length(area_light.u) * length(area_light.v) / (float)AREA_SMP;
            col += throughput * surf_color * area_light.emission * sh_accum * area_scale;

            float diff = max(dot(n, light_dir), 0.f);
            if (diff > 0) {
                float3 shad_ro = p + n * 0.003f;
                float st = 1e10f;
                int sidx = -1;
                for (int i = 0; i < num_spheres; i++) {
                    float h = hit_sphere(spheres[i], shad_ro, light_dir);
                    if (h > 0 && h < st) { st = h; sidx = i; }
                }
                float shadow = (sidx < 0) ? 1.f : 0.2f;
                col += throughput * surf_color * diff * 0.3f * shadow;

                float3 h_vec = normalize(light_dir - rd);
                float spec = pow(max(dot(n, h_vec), 0.f), 64);
                col += throughput * float3(1, 1, 1) * spec * 0.2f * shadow;
            }

            // Sub-bounce path
            if (reflectivity > 0) {
                float3 refl = reflect(rd, n);
                if (roughness > 0.001f) {
                    float3 ru, rv;
                    basis_from_normal(refl, ru, rv);
                    float2 rv2 = ign2(float2((float)(x * 500 + s * 50), (float)(y * 200 + s * 7)));
                    float2 disk = concentric_disk(rv2) * roughness;
                    refl = normalize(refl + ru * disk.x + rv * disk.y);
                }
                col += trace_path(spheres, num_spheres, light_dir, area_light,
                    p + n * 0.002f, refl, throughput * reflectivity, 1,
                    x, y, s, scramble);
            }
        }
    }

    col = col / (float)(SAMPLES * SAMPLES);
    col = aces(col * 1.3f);
    col = clamp(col, 0.f, 1.f);

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
@property(strong) id<MTLComputePipelineState> pipeline;
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
