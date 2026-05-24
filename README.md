# cpp-vibe-code

just a repo i use to vibe code random shit when i'm bored. i have genuinely no idea what any of this does but it compiles so we ball.

## Projects

### [liquid](./projects/liquid)

Eulerian grid-based fluid simulation with semi-Lagrangian advection and OpenMP parallelism.

### [blackhole-sim](./projects/blackhole-sim)

Binary black hole collision with accretion disk, gravitational lensing, and LIGO-style audio chirp.

### [constexpr-tracer](./projects/constexpr-tracer)

GPU-accelerated Monte Carlo path tracer on Apple Metal compute shaders.

---

## Building

Each project has its own `Makefile`. From the project directory:

```bash
cd projects/liquid
make && ./liquid
```

**Dependencies:** SDL2 + SDL2_ttf (install via Homebrew: `brew install sdl2 sdl2_ttf`). The constexpr-tracer additionally requires the Metal and Foundation frameworks (included with macOS).
