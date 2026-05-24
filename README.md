# C++-vibe

A curated collection of C++ simulation and visualization projects, built with modern C++17, SDL2, and OpenGL.

## Projects

### [form](./projects/form)

Real-time 3D Mandelbulb fractal explorer powered by software ray marching — no GPU needed.

**Features:**
- Ray-marched Mandelbulb set (power-2 through power-12) with infinite geometric detail
- Orbit-trap coloring producing vibrant, organic multi-spectral palettes
- Real-time interactive camera with orbital controls and auto-rotation
- Dynamic ambient occlusion and soft shadow approximation
- Adjustable iteration depth and fractal power at runtime

**Controls:** `Drag` Orbit · `Scroll` Zoom · `+/-` Power · `↑/↓` Iterations · `Space` Auto-rotate

---

### [blackhole-sim](./projects/blackhole-sim)

Real-time black hole collision simulation with N-body gravity, 3D accretion disk rendering, gravitational lensing, LIGO-inspired audio chirp, and a Keplerian solar system mode.

**Features:**
- Binary black hole inspiral, merger, and ring-down
- 3D accretion disk with temperature gradient and Doppler beaming
- Photon ring at 1.5× Schwarzschild radius
- Gravitational wave spectrogram
- Screen shake, kilonova particle burst, polar jets
- Solar system mode with all 8 planets

**Controls:** `1` Black Holes · `2` Solar System · `Space` Pause · `Drag` Pan · `Scroll` Zoom

---

## Building

Each project has its own `Makefile`. From the project directory:

```bash
cd projects/form
make && ./form
```

**Dependencies:** SDL2 (install via Homebrew: `brew install sdl2`). Some projects may also require `sdl2_ttf`.
