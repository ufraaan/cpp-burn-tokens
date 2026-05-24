#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cmath>
#include <chrono>

static NSString* shader_src = @R"(
#include <metal_stdlib>
using namespace metal;

// ─── Hash-based RNG ─────────────────────────────────────────────────────
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

// ─── Monte Carlo π kernel ───────────────────────────────────────────────
// Each thread writes its local hit count to per_thread_hits[tid].
// The CPU sums the buffer on readback (avoids GPU atomics).
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
        if (x * x + y * y <= 1.f)
            ++hits;
    }

    per_thread_hits[tid] = hits;
}
)";

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        // Parse optional args: ./pi-montecarlo [samples_per_thread]
        uint samples_per_thread = 1024;
        if (argc > 1) samples_per_thread = (uint)atol(argv[1]);
        if (samples_per_thread < 1) samples_per_thread = 1;

        // Metal setup
        NSError* err = nil;
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) { fprintf(stderr, "No Metal device found\n"); return 1; }

        id<MTLLibrary> lib = [device newLibraryWithSource:shader_src options:nil error:&err];
        if (!lib) { fprintf(stderr, "Shader error: %s\n", err.localizedDescription.UTF8String); return 1; }

        id<MTLFunction> func = [lib newFunctionWithName:@"pi_kernel"];
        id<MTLCommandQueue> queue = [device newCommandQueue];

        // Build a ~1M thread grid
        NSUInteger tg_max = device.maxThreadsPerThreadgroup.width;
        uint tg_size = (uint)MIN(tg_max, 256);

        // Pick a grid size that's a power of two and at least 256K
        uint grid_size = 1;
        while ((uint64_t)grid_size * tg_size < (uint64_t)(256 * 1024))
            grid_size *= 2;
        while ((uint64_t)grid_size * samples_per_thread < (uint64_t)(8ull * 1000 * 1000 * 1000))
            grid_size *= 2;
        // Cap grid so total buffer stays reasonable (max ~64M threads → 256 MB)
        if (grid_size > 64 * 1024 * 1024)
            grid_size = 64 * 1024 * 1024;

        uint64_t total_samples = (uint64_t)grid_size * samples_per_thread;

        // Buffers
        id<MTLBuffer> buf_hits = [device newBufferWithLength:grid_size * sizeof(uint)
                                                     options:MTLResourceStorageModeShared];
        memset(buf_hits.contents, 0, grid_size * sizeof(uint));

        id<MTLBuffer> buf_samples = [device newBufferWithBytes:&samples_per_thread
                                                        length:sizeof(uint)
                                                       options:MTLResourceStorageModeShared];
        uint seed = (uint)time(NULL);
        id<MTLBuffer> buf_seed = [device newBufferWithBytes:&seed
                                                     length:sizeof(uint)
                                                    options:MTLResourceStorageModeShared];

        id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:func
                                                                                error:&err];
        if (!pso) { fprintf(stderr, "PSO error: %s\n", err.localizedDescription.UTF8String); return 1; }

        // Warm-up run (compiles + caches PSO, discard results)
        id<MTLCommandBuffer> warmup_cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> warmup_enc = [warmup_cb computeCommandEncoder];
        [warmup_enc setComputePipelineState:pso];
        [warmup_enc setBuffer:buf_hits    offset:0 atIndex:0];
        [warmup_enc setBuffer:buf_samples offset:0 atIndex:1];
        [warmup_enc setBuffer:buf_seed    offset:0 atIndex:2];
        [warmup_enc dispatchThreads:MTLSizeMake(65536, 1, 1)
               threadsPerThreadgroup:MTLSizeMake(tg_size, 1, 1)];
        [warmup_enc endEncoding];
        [warmup_cb commit];
        [warmup_cb waitUntilCompleted];
        memset(buf_hits.contents, 0, grid_size * sizeof(uint));

        // Timed run
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

        // CPU sum of per-thread hits
        uint32_t* hits = (uint32_t*)buf_hits.contents;
        uint64_t total_hits = 0;
        for (uint i = 0; i < grid_size; ++i)
            total_hits += hits[i];

        double pi_est = 4.0 * (double)total_hits / (double)total_samples;
        double error = fabs(pi_est - M_PI);

        printf("╔══════════════════════════════════════╗\n");
        printf("║      Monte Carlo π — GPU Metal       ║\n");
        printf("╚══════════════════════════════════════╝\n");
        printf("\n");
        printf("  π ≈      %.12f\n", pi_est);
        printf("  error:   ±%.12f\n", error);
        printf("  samples: %llu\n", (unsigned long long)total_samples);
        printf("  threads: %u\n", grid_size);
        printf("  samples/thread: %u\n", samples_per_thread);
        printf("  GPU time: %.3f s\n", elapsed);
        printf("  throughput: %.2f G samples/s\n",
               (double)total_samples / elapsed / 1e9);
        printf("\n");
        printf("  expected π: %.12f\n", M_PI);
        printf("  hits: %llu / %llu (%.6f%%)\n",
               (unsigned long long)total_hits, (unsigned long long)total_samples,
               100.0 * (double)total_hits / (double)total_samples);
    }
    return 0;
}
