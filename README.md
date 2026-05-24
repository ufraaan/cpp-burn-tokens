# C++-vibe

A curated collection of C++ simulation and visualization projects, built with modern C++17, SDL2, and OpenGL.

## Projects

### [liquid](./projects/liquid)

Real-time 2D fluid simulation using Smoothed Particle Hydrodynamics (SPH).

**Features:**
- Interactive SPH solver with pressure, viscosity, and surface tension forces
- Grid-based spatial hashing for fast neighbor lookup
- Glowing particle rendering with velocity-based color mapping
- Click to spray, splash, and pour fluid in real time
- Gravity toggle for zero-G experiments

**Controls:** `Click+Drag` Spray fluid · `R` Reset · `Space` Splash · `G` Toggle gravity

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
cd projects/liquid
make && ./liquid
```

**Dependencies:** SDL2 (install via Homebrew: `brew install sdl2`). Some projects may also require `sdl2_ttf`.
