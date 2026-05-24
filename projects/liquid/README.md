# liquid — Eulerian Grid Fluid Simulation

Real-time 2D fluid simulation using a MAC grid with semi-Lagrangian advection, Jacobi pressure projection, and OpenMP parallelism.

## Features

- **Eulerian Navier-Stokes solver** on a 600×600 MAC grid
- Semi-Lagrangian advection with iterative reflective wall boundary conditions
- Jacobi pressure projection with divergence-free constraint
- OpenMP-parallelized grid operations for multi-core CPUs
- Density-dependent gravity (buoyancy-driven flow)
- Specular gradient-shimmer flat shader
- TTF metadata HUD (resolution, grid size, FPS, simulation time)
- 960×960 display window

## Controls

| Key | Action |
|---|---|
| `Click + Drag` | Spray fluid |
| `Space` | Splash burst |
| `R` | Reset simulation |
| `G` | Toggle gravity |
| `Esc` | Quit |

## Build

```bash
cd projects/liquid
make && ./liquid
```

## Dependencies

- SDL2 (`brew install sdl2`)
- SDL2_ttf (`brew install sdl2_ttf`)
- OpenMP (included with Apple Clang via `-Xclang -fopenmp`; libomp via `brew install libomp`)
