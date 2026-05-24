# cpp-vibe-code

just a repo i use to vibe code random shit when i'm bored. i have genuinely no idea what any of this does but it compiles so we ball.

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

### [constexpr-tracer](./projects/constexpr-tracer)

A GPU-accelerated ray tracer powered by Metal compute shaders. All rendering runs on your GPU at native display resolution, delivering production-quality results in under half a second.

**Features:**
- 8 spheres with diverse materials: matte, metallic, **chrome**, **glass**, **golden**, **glossy**
- **Glass sphere** with Schlick Fresnel, Snell refraction, total internal reflection, Beer's law absorption
- **4×4 supersampled anti-aliasing** (16 rays/pixel) for sub-pixel smoothness
- **Rectangular area light** with 24× soft shadow sampling, inverse-square falloff
- **Depth of field** with aperture disk sampling and focal distance
- **Glossy microfacet reflections** with configurable roughness per material
- **Checkerboard ground plane** with reflections visible in every sphere
- **ACES filmic tone mapping** for proper color grading
- Up to **8 recursive bounces** for complex light paths
- **Metadata HUD overlay** (Blender-style render stamp) with resolution, settings, and render time
- Renders at **native Retina resolution** (3420×2146 on M5 MacBook Air) in ~480 ms
- Completely deterministic — no random seed variance, same output every run

**Controls:** `Esc` Quit

**Build:**
```bash
cd projects/constexpr-tracer
make && ./cpptracer
```

---

## Building

Each project has its own `Makefile`. From the project directory:

```bash
cd projects/liquid
make && ./liquid
```

**Dependencies:** SDL2 + SDL2_ttf (install via Homebrew: `brew install sdl2 sdl2_ttf`). The constexpr-tracer additionally requires the Metal and Foundation frameworks (included with macOS).
