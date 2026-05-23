# C++-vibe

A curated collection of C++ simulation and visualization projects, built with modern C++17 and SDL2.

## Projects

### [voxel](./projects/voxel)

3D procedural terrain explorer with real-time OpenGL rendering, Perlin noise landscape generation, and interactive orbital camera controls.

**Features:**
- 160×160 vertex terrain mesh with fractal Brownian motion (7 octaves)
- Height-based coloring: bathymetry → shoreline → grassland → forest → rock → snow
- OpenGL lighting with directional illumination and exponential fog
- Orbital camera with smooth interpolation, auto-rotation, and mouse drag
- Wireframe mode, water plane, and real-time terrain regeneration

**Controls:** `Drag` Orbit · `Scroll` Zoom · `R` Regenerate · `W` Wireframe · `Space` Auto-rotate

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
cd projects/blackhole-sim
make && ./bhs
```

**Dependencies:** SDL2, SDL2_ttf (install via Homebrew: `brew install sdl2 sdl2_ttf`)
