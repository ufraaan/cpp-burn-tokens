# pi-montecarlo — GPU-Accelerated π Approximation via Monte Carlo

Computes π by throwing random (x, y) points at a unit quarter-circle on the GPU. Each of millions of Metal compute threads runs a hash-based RNG and accumulates hits inside the circle. The CPU sums per-thread results and prints the estimate.

## Build & Run

```bash
cd projects/pi-montecarlo
make && ./pi-montecarlo
```

Optionally set samples per thread:

```bash
./pi-montecarlo 4096
```

## Output

```
╔══════════════════════════════════════╗
║      Monte Carlo π — GPU Metal       ║
╚══════════════════════════════════════╝

  π ≈      3.141596443951
  error:   ±0.000003790361
  samples: 4294967296
  threads: 4194304
  samples/thread: 1024
  GPU time: 0.043 s
  throughput: 100.63 G samples/s
```

## Performance

On Apple M5 (10-core GPU), ~8.5 billion samples complete in ~74 ms (116 G samples/s). Error scales as 1/√N, reaching ~4 decimal places of accuracy at 4 billion samples.

## Dependencies

- Metal.framework + Foundation.framework (included with macOS)

No SDL2, no graphics, no GUI — pure GPU compute.
