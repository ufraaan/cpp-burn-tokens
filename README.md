# C++-vibe

A curated collection of C++ simulation and visualization projects, built with modern C++17 and SDL2.

## Projects

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
