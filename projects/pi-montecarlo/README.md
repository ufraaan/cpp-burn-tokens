# pi-montecarlo — GPU-Accelerated π Approximation via Monte Carlo

Computes π by throwing random (x, y) points at a unit quarter-circle on the GPU. Each of 4 million Metal compute threads runs a Wang hash RNG and accumulates hits inside the circle. The CPU sums per-thread results and prints the estimate.

No SDL2, no graphics, no GUI — pure GPU compute.

## Build & Run

```bash
cd projects/pi-montecarlo
make && ./pi-montecarlo
```

### Usage

```
./pi-montecarlo [samples_per_thread] [duration_sec] [grid_k]
```

| Argument | Default | Description |
|---|---|---|
| `samples_per_thread` | 1024 | Number of Monte Carlo iterations per GPU thread |
| `duration_sec` | 0 | If > 0, run in a timed loop for this many seconds |
| `grid_k` | auto | Thread grid size in K (e.g., 4096 = 4,194,304 threads) |

- With no args: runs a characterization sweep across multiple sample counts
- With `duration_sec`: runs flat-out for the specified time, accumulating across dispatches
- With `grid_k`: overrides the default 4M-thread grid

## Performance Benchmarks

Tested on **Apple M5 (10-core GPU)**, 4,194,304 thread grid, Wang hash RNG.

### Workload Sweep

| Samples/Thread | Total Samples | Time (s) | G samp/s | π | Error |
|---|---|---|---|---|---|
| 1 | 4,194,304 | 0.0007 | 5.66 | 3.142303466797 | 7.11e-04 |
| 4 | 16,777,216 | 0.0014 | 11.90 | 3.141502141953 | 9.05e-05 |
| 16 | 67,108,864 | 0.0030 | 22.36 | 3.141873836517 | 2.81e-04 |
| 64 | 268,435,456 | 0.0095 | 28.33 | 3.141604259610 | 1.16e-05 |
| 256 | 1,073,741,824 | 0.0161 | 66.52 | 3.141674421728 | 8.18e-05 |
| 1024 | 4,294,967,296 | 0.0328 | 131.07 | 3.141598906368 | 6.25e-06 |
| 4096 | 17,179,869,184 | 0.1298 | 132.31 | 3.141593169654 | 5.16e-07 |
| 16384 | 68,719,476,736 | 0.5187 | 132.49 | 3.141591539315 | 1.11e-06 |
| **65536** | **274,877,906,944** | **2.0481** | **134.21** | **3.141592502914** | **1.51e-07** |

### Sustained 5-Minute Run

| Metric | Value |
|---|---|
| Total samples | 33,359,010,988,032 |
| GPU time | 300.01 s |
| Throughput | 111.19 G samples/s |
| π | 3.141573241912 |
| Error | ±1.94e-05 |
| Iterations | 7,767 dispatches |

### Observations

- **Throughput scales with work per thread.** At 1 sample/thread the kernel is launch-latency-bound (5.66 G/s). By 1024 samples/thread the GPU saturates at ~131-134 G/s.
- **The 5-min sustained run averages 111 G/s** — the gap from 134 G/s is CPU-side readback and summation of the 16 MB per-thread hit buffer between dispatches.
- **Error scales as 1/√N.** Monte Carlo standard error = σ/√N with σ ≈ 0.433 for the quarter-circle estimator. At 274B samples the error reaches 1.5e-7 (π correct to 6 decimal places).

## Dependencies

- Metal.framework + Foundation.framework (included with macOS)
