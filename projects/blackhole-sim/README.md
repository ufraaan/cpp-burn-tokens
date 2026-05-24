# blackhole-sim — Binary Black Hole Collision Simulator

Real-time N-body black hole collision with relativistic accretion disk rendering, gravitational lensing, LIGO-inspired audio chirp, and a Keplerian solar system mode.

## Features

- **Binary black hole inspiral, merger, and ring-down** with N-body gravity
- **3D accretion disk** with temperature gradient (inner to outer) and Doppler beaming
- **Photon ring** at the innermost stable circular orbit (1.5× Schwarzschild radius)
- **Gravitational lensing** of background stars and the accretion disk
- **Einstein ring** visible when black holes are aligned with a background source
- **Gravitational wave spectrogram** visualization during merger
- **LIGO-inspired audio chirp** — frequency and amplitude ramp up to merger, then ring-down
- **Kilonova particle burst** during ring-down phase
- **Polar jets** emitted along the rotation axis during merger
- **Screen shake** on peak gravitational wave emission
- **Solar system mode** with all 8 planets orbiting the Sun at Keplerian velocities
- Canvas drag, pan, and zoom with mouse

## Controls

| Key | Action |
|---|---|
| `1` | Black hole collision mode |
| `2` | Solar system mode |
| `Space` | Pause / Resume |
| `R` | Reset simulation |
| `+` / `=` | Increase simulation speed (×1.5) |
| `-` | Decrease simulation speed (÷1.5) |
| `G` | Toggle grid overlay |
| `S` | Save screenshot |
| `F` | Toggle fullscreen |
| `Drag` | Pan camera |
| `Scroll` | Zoom in / out |
| `Esc` | Quit |

## Build

```bash
cd projects/blackhole-sim
make && ./blackhole-sim
```

## Dependencies

- SDL2 (`brew install sdl2`)
- SDL2_ttf (`brew install sdl2_ttf`)
