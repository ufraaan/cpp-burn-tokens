#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <string>

static NSString* shader_src = @R"(
#include <metal_stdlib>
using namespace metal;

uint wang_hash(uint x) {
    x ^= x >> 16;
    x *= 0x45d9f3bU;
    x ^= x >> 16;
    x *= 0x45d9f3bU;
    x ^= x >> 16;
    return x;
}

float rng_float(thread uint& state) {
    state = wang_hash(state);
    return float(state & 0xffffff) * (1.f / 16777215.f);
}

kernel void pi_kernel(
    device uint* per_thread_hits [[buffer(0)]],
    constant uint& samples_per_thread [[buffer(1)]],
    constant uint& rng_seed [[buffer(2)]],
    uint tid [[thread_position_in_grid]])
{
    uint state = rng_seed ^ (tid * 2654435761U);
    uint hits = 0;
    for (uint i = 0; i < samples_per_thread; ++i) {
        float x = rng_float(state);
        float y = rng_float(state);
        if (x * x + y * y <= 1.f) ++hits;
    }
    per_thread_hits[tid] = hits;
}
)";

// ─── Helpers ───────────────────────────────────────────────────────────

struct Result {
    uint64_t samples;
    uint64_t hits;
    double   time_s;
    double   pi;
    double   error;
    double   throughput_gbs;
};

static uint pick_grid_size(uint samples_per_thread, uint tg_size, uint64_t min_samples = 0) {
    uint gs = 1;
    if (min_samples > 0) {
        while ((uint64_t)gs * samples_per_thread < min_samples)
            gs *= 2;
    } else {
        while ((uint64_t)gs * tg_size < 256 * 1024)
            gs *= 2;
        while ((uint64_t)gs * samples_per_thread < (uint64_t)8ull * 1000 * 1000 * 1000)
            gs *= 2;
    }
    if (gs > 64 * 1024 * 1024) gs = 64 * 1024 * 1024;
    return gs;
}

static Result run_single(id<MTLComputePipelineState> pso,
                         id<MTLCommandQueue> queue,
                         id<MTLBuffer> buf_hits,
                         id<MTLBuffer> buf_samples,
                         id<MTLBuffer> buf_seed,
                         uint grid_size, uint tg_size,
                         uint samples_per_thread)
{
    memset(buf_hits.contents, 0, grid_size * sizeof(uint));

    auto t0 = std::chrono::high_resolution_clock::now();
    id<MTLCommandBuffer> cb = [queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
    [enc setComputePipelineState:pso];
    [enc setBuffer:buf_hits    offset:0 atIndex:0];
    [enc setBuffer:buf_samples offset:0 atIndex:1];
    [enc setBuffer:buf_seed    offset:0 atIndex:2];
    [enc dispatchThreads:MTLSizeMake(grid_size, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg_size, 1, 1)];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    uint64_t total_samples = (uint64_t)grid_size * samples_per_thread;

    uint32_t* hits = (uint32_t*)buf_hits.contents;
    uint64_t total_hits = 0;
    for (uint i = 0; i < grid_size; ++i) total_hits += hits[i];

    double pi_est = 4.0 * (double)total_hits / (double)total_samples;
    return { total_samples, total_hits, elapsed, pi_est,
             fabs(pi_est - M_PI), (double)total_samples / elapsed / 1e9 };
}

// ─── Main ──────────────────────────────────────────────────────────────

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        // Args: ./pi-montecarlo [samples_per_thread] [duration_sec] [grid_k]
        uint samples_per_thread = 1024;
        double duration_sec = 0;
        uint grid_k = 0; // 0 = auto

        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg == "--help" || arg == "-h") {
                printf("Usage: %s [samples_per_thread=%u] [duration_sec=0] [grid_k=auto]\n",
                       argv[0], samples_per_thread);
                printf("  duration_sec = 0  → single shot\n");
                printf("  duration_sec > 0  → timed run (looping dispatches)\n");
                printf("  grid_k = 0 → auto; 1,2,4,8,16,... sets grid size in K\n");
                return 0;
            }
        }

        if (argc > 1) samples_per_thread = (uint)atol(argv[1]);
        if (argc > 2) duration_sec = atof(argv[2]);
        if (argc > 3) grid_k = (uint)atol(argv[3]);
        if (samples_per_thread < 1) samples_per_thread = 1;

        // Metal setup
        NSError* err = nil;
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) { fprintf(stderr, "No Metal device found\n"); return 1; }

        printf("GPU: %s\n", device.name.UTF8String);
        printf("\n");

        id<MTLLibrary> lib = [device newLibraryWithSource:shader_src options:nil error:&err];
        if (!lib) { fprintf(stderr, "Shader error: %s\n", err.localizedDescription.UTF8String); return 1; }

        id<MTLFunction> func = [lib newFunctionWithName:@"pi_kernel"];
        id<MTLCommandQueue> queue = [device newCommandQueue];

        NSUInteger tg_max = device.maxThreadsPerThreadgroup.width;
        uint tg_size = (uint)MIN(tg_max, 256);

        id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:func error:&err];
        if (!pso) { fprintf(stderr, "PSO error: %s\n", err.localizedDescription.UTF8String); return 1; }

        // Determine grid size (fixed 4M for sweep/timed, can override via arg 3)
        uint grid_size;
        if (grid_k > 0) {
            grid_size = grid_k * 1024;
        } else {
            grid_size = 4194304; // 4M threads
        }

        // Buffers (shared = CPU + GPU can both access)
        uint64_t buf_len = (uint64_t)grid_size * sizeof(uint);
        printf("  grid: %u threads  (%llu MB buffer)\n",
               grid_size, (unsigned long long)(buf_len / (1024*1024)));
        printf("\n");

        id<MTLBuffer> buf_hits = [device newBufferWithLength:buf_len
                                                     options:MTLResourceStorageModeShared];
        id<MTLBuffer> buf_samples = [device newBufferWithBytes:&samples_per_thread
                                                        length:sizeof(uint)
                                                       options:MTLResourceStorageModeShared];
        uint seed = (uint)time(NULL);
        id<MTLBuffer> buf_seed = [device newBufferWithBytes:&seed
                                                     length:sizeof(uint)
                                                    options:MTLResourceStorageModeShared];

        // Warm-up
        memset(buf_hits.contents, 0, grid_size * sizeof(uint));
        id<MTLCommandBuffer> wcb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> wenc = [wcb computeCommandEncoder];
        [wenc setComputePipelineState:pso];
        [wenc setBuffer:buf_hits    offset:0 atIndex:0];
        [wenc setBuffer:buf_samples offset:0 atIndex:1];
        [wenc setBuffer:buf_seed    offset:0 atIndex:2];
        [wenc dispatchThreads:MTLSizeMake(65536, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(tg_size, 1, 1)];
        [wenc endEncoding];
        [wcb commit];
        [wcb waitUntilCompleted];

        // ── Mode: timed run ────────────────────────────────────────────
        if (duration_sec > 0) {
            printf("╔══════════════════════════════════════════════╗\n");
            printf("║       Timed Run (%.0f s)                     ║\n", duration_sec);
            printf("╚══════════════════════════════════════════════╝\n");
            printf("\n");

            uint64_t grand_hits = 0;
            uint64_t grand_samples = 0;
            int iters = 0;
            auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::duration<double>(duration_sec);

            while (std::chrono::steady_clock::now() < deadline) {
                memset(buf_hits.contents, 0, grid_size * sizeof(uint));

                id<MTLCommandBuffer> cb = [queue commandBuffer];
                id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
                [enc setComputePipelineState:pso];
                [enc setBuffer:buf_hits    offset:0 atIndex:0];
                [enc setBuffer:buf_samples offset:0 atIndex:1];
                [enc setBuffer:buf_seed    offset:0 atIndex:2];
                [enc dispatchThreads:MTLSizeMake(grid_size, 1, 1)
               threadsPerThreadgroup:MTLSizeMake(tg_size, 1, 1)];
                [enc endEncoding];
                [cb commit];
                [cb waitUntilCompleted];

                uint32_t* hits = (uint32_t*)buf_hits.contents;
                uint64_t iter_hits = 0;
                for (uint i = 0; i < grid_size; ++i) iter_hits += hits[i];

                grand_hits += iter_hits;
                grand_samples += (uint64_t)grid_size * samples_per_thread;
                iters++;
            }

            auto now = std::chrono::steady_clock::now();
            double actual_time = std::chrono::duration<double>(now - (deadline - std::chrono::duration<double>(duration_sec))).count();

            double pi_est = 4.0 * (double)grand_hits / (double)grand_samples;
            double error = fabs(pi_est - M_PI);

            printf("  π ≈         %.12f\n", pi_est);
            printf("  error:      ±%.12f\n", error);
            printf("  samples:    %llu\n", (unsigned long long)grand_samples);
            printf("  iterations: %d\n", iters);
            printf("  GPU time:   %.3f s\n", actual_time);
            printf("  throughput: %.2f G samples/s\n",
                   (double)grand_samples / actual_time / 1e9);
            printf("  expected π: %.12f\n", M_PI);
            printf("\n");
            return 0;
        }

        // ── Mode: characterization sweep ───────────────────────────────
        printf("╔══════════════════════════════════════════════╗\n");
        printf("║        Characterization Sweep                ║\n");
        printf("╚══════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  Thread grid: %u  (%u per threadgroup)\n", grid_size, tg_size);
        printf("\n");

        std::vector<uint> sps_values = {1, 4, 16, 64, 256, 1024, 4096, 16384, 65536};

        printf("  %-20s  %-14s  %-10s  %-10s  %-16s  %s\n",
               "Samples/Thread", "Total Samples", "Time (s)", "G samp/s",
               "pi", "Error");
        printf("  %s\n", std::string(90, '-').c_str());

        for (uint sps : sps_values) {
            samples_per_thread = sps;
            *(uint*)buf_samples.contents = sps;

            Result r = run_single(pso, queue, buf_hits, buf_samples, buf_seed,
                                  grid_size, tg_size, sps);

            printf("  %-20u  %-14llu  %-10.4f  %-10.2f  %-16.12f  %.2e\n",
                   sps,
                   (unsigned long long)r.samples,
                   r.time_s, r.throughput_gbs,
                   r.pi, r.error);
        }

        printf("\n");
        printf("  Sweep complete.\n");
        printf("\n");
    }
    return 0;
}
