#include "physics.hpp"
#include <algorithm>
#include <cmath>

static double randDouble() { return (double)std::rand() / RAND_MAX; }

Simulation::Simulation() {
    initStars();
    initGrid();
    reset();
}

void Simulation::reset() {
    blackHoles.clear();
    particles.clear();
    gravWaves.clear();
    kilonova.clear();
    jets.clear();
    freqHistory.clear();
    merged = false;
    mergerTime = 0.0;
    mergerPos = {0, 0};
    maxOrbitFreq = 0.0;
    shakeIntensity = 0.0;

    blackHoles.push_back({{-15.0, 0.0}, {0.0, -12.0}, 10.0, 3.0});
    blackHoles.push_back({{15.0, 0.0}, {0.0, 12.0}, 11.0, 3.2});
    time = 0.0;
}

void Simulation::addBlackHole(Vec2 pos, Vec2 vel, double mass, double radius) {
    blackHoles.push_back({pos, vel, mass, radius, false, 0.0, 0.0});
}

void Simulation::switchScene(SceneMode mode) {
    sceneMode = mode;
    planets.clear();
    blackHoles.clear();
    particles.clear();
    kilonova.clear();
    jets.clear();
    gravWaves.clear();
    freqHistory.clear();
    merged = false;
    shakeIntensity = 0.0;

    if (mode == SceneMode::SolarSystem) {
        // Orbital period scaling: 1 year ≈ 6 simulation seconds
        // angularVelocity = 2*pi / (period_years * 6.0)
        struct PlanetData { const char* name; double orbit; double rad; int r, g, b; double period; };
        PlanetData data[] = {
            {"Mercury",  12,   0.8,  180, 180, 190, 0.241},
            {"Venus",    18,   1.2,  220, 180, 130, 0.615},
            {"Earth",    25,   1.3,  100, 160, 255, 1.0},
            {"Mars",     35,   0.9,  220, 100, 80,  1.881},
            {"Jupiter",  60,   3.5,  230, 180, 100, 11.86},
            {"Saturn",   85,   3.0,  210, 190, 120, 29.46},
            {"Uranus",   120,  1.8,  140, 210, 220, 84.01},
            {"Neptune",  155,  1.7,  100, 140, 220, 164.8},
        };

        for (auto& d : data) {
            Planet p;
            p.orbitRadius = d.orbit;
            p.radius = d.rad;
            p.angle = randDouble() * 6.2832;
            p.angularVelocity = 6.2832 / (d.period * 6.0);
            p.r = d.r; p.g = d.g; p.b = d.b;
            p.name = d.name;
            p.pos = Vec2{p.orbitRadius * std::cos(p.angle), p.orbitRadius * std::sin(p.angle)};
            planets.push_back(p);
        }
        time = 0.0;
    } else {
        reset();
    }
}

void Simulation::updateSolarSystem(double dt) {
    for (auto& p : planets) {
        p.angle += p.angularVelocity * dt;
        p.pos = Vec2{p.orbitRadius * std::cos(p.angle), p.orbitRadius * std::sin(p.angle)};
    }
}

void Simulation::initStars() {
    stars.resize(config.numStars);
    for (auto& s : stars) {
        s.pos.x = (std::rand() % 4000 - 2000) / 10.0;
        s.pos.y = (std::rand() % 3000 - 1500) / 10.0;
        s.brightness = 0.2 + 0.8 * randDouble();
        s.twinklePhase = randDouble() * 6.2832;
        s.colorIdx = std::rand() % 3;
    }
}

void Simulation::initGrid() {
    grid.clear();
    int halfExt = config.gridExtent;
    int spacing = config.gridSpacing;
    for (int gx = -halfExt; gx <= halfExt; gx += spacing)
        for (int gy = -halfExt; gy <= halfExt; gy += spacing)
            grid.push_back({Vec2{(double)gx, (double)gy}, Vec2{(double)gx, (double)gy}});
}

void Simulation::update(double dt) {
    if (paused) return;
    dt *= speed;

    if (sceneMode == SceneMode::SolarSystem) {
        updateSolarSystem(dt);
        time += dt;
        return;
    }

    for (int s = 0; s < config.substeps; s++) {
        updateBlackHoles(dt / config.substeps);
        checkMerger();
        updateParticles(dt / config.substeps);
        updateKilonova(dt / config.substeps);
        updateJets(dt / config.substeps);
        spawnParticles();
        spawnJets(dt / config.substeps);
        addTrailPoints();
    }
    updateGravWaves();
    updateGrid();
    recordFrequency();
    time += dt;

    if (shakeIntensity > 0) shakeIntensity *= 0.94;
}

void Simulation::updateBlackHoles(double dt) {
    for (size_t i = 0; i < blackHoles.size(); i++) {
        auto& bh = blackHoles[i];
        if (bh.merged && bh.mergeProgress >= 1.0) continue;

        Vec2 force{0, 0};
        for (size_t j = 0; j < blackHoles.size(); j++) {
            if (i == j) continue;
            auto& other = blackHoles[j];
            if (other.merged && other.mergeProgress >= 1.0) continue;

            Vec2 diff = other.pos - bh.pos;
            double r2 = diff.len2();
            if (r2 < 0.01) r2 = 0.01;
            double r = std::sqrt(r2);
            double F = config.G * bh.mass * other.mass / (r2 + 0.5);
            force = force + (diff / r) * F;
        }

        if (!bh.merged) {
            bh.vel = bh.vel + (force / bh.mass) * dt;
            bh.pos = bh.pos + bh.vel * dt;
            bh.dopplerAngle = std::atan2(bh.vel.y, bh.vel.x);
        }

        if (bh.merged && bh.mergeProgress < 1.0) {
            bh.mergeProgress += 0.002;
            bh.radius *= 0.998;
        }
    }
}

void Simulation::checkMerger() {
    if (merged) return;

    for (size_t i = 0; i < blackHoles.size(); i++) {
        for (size_t j = i + 1; j < blackHoles.size(); j++) {
            auto& a = blackHoles[i];
            auto& b = blackHoles[j];
            if (a.merged || b.merged) continue;

            double dist = (a.pos - b.pos).len();
            if (dist < (a.radius + b.radius) * 1.2) {
                merged = true;
                mergerTime = time;
                mergerPos = (a.pos * a.mass + b.pos * b.mass) / (a.mass + b.mass);

                double totalMass = a.mass + b.mass;
                Vec2 com = mergerPos;
                Vec2 comVel = (a.vel * a.mass + b.vel * b.mass) / totalMass;
                double remnantRadius = std::pow(totalMass, 0.7) * 1.5;

                for (int w = 0; w < 8; w++)
                    gravWaves.push_back({mergerPos, time + w * 0.03, 1.0});

                a.pos = com;
                a.vel = comVel;
                a.mass = totalMass;
                a.radius = remnantRadius;
                a.merged = true;
                a.dopplerAngle = std::atan2(comVel.y, comVel.x);

                b.merged = true;
                b.mergeProgress = 1.0;

                spawnKilonova();
                shakeIntensity = 20.0;
                break;
            }
        }
        if (merged) break;
    }
}

void Simulation::spawnKilonova() {
    for (int i = 0; i < 1200; i++) {
        double theta = randDouble() * 6.2832;
        double phi = std::acos(2.0 * randDouble() - 1.0);
        double speed = 15.0 + randDouble() * 35.0;

        KilonovaParticle kp;
        kp.pos = mergerPos;
        kp.vel = Vec2{speed * std::sin(phi) * std::cos(theta),
                      speed * std::sin(phi) * std::sin(theta)};
        kp.maxLife = 0.5 + randDouble() * 1.5;
        kp.lifetime = kp.maxLife;

        double t = randDouble();
        if (t < 0.4)      { kp.r = 255; kp.g = 200; kp.b = 80; }
        else if (t < 0.7) { kp.r = 200; kp.g = 150; kp.b = 255; }
        else if (t < 0.9) { kp.r = 255; kp.g = 255; kp.b = 255; }
        else              { kp.r = 80;  kp.g = 180; kp.b = 255; }
        kilonova.push_back(kp);
    }
}

void Simulation::spawnJets(double dt) {
    if (!merged) return;
    if (jets.size() > 800) return;

    auto& remnant = blackHoles[0];
    if (!remnant.merged) return;

    // Determine jet axis (perpendicular to pre-merger orbital plane, approximated as perpendicular to velocity)
    double jetAngle = remnant.dopplerAngle + 1.5708; // perpendicular to motion

    for (int i = 0; i < 3; i++) {
        double spread = 0.15;
        double angle = jetAngle + (randDouble() - 0.5) * spread;
        double upSpeed = 20.0 + randDouble() * 15.0;

        JetParticle jp;
        jp.pos = remnant.pos + Vec2{std::cos(angle), std::sin(angle)} * remnant.radius * 0.5;
        jp.vel = Vec2{std::cos(angle), std::sin(angle)} * upSpeed + remnant.vel * 0.3;
        jp.maxLife = 4.0 + randDouble() * 2.0;
        jp.lifetime = jp.maxLife;
        jp.r = 255; jp.g = 200; jp.b = 255;
        jets.push_back(jp);

        // Opposing jet
        angle = jetAngle + 3.14159 + (randDouble() - 0.5) * spread;
        jp.pos = remnant.pos + Vec2{std::cos(angle), std::sin(angle)} * remnant.radius * 0.5;
        jp.vel = Vec2{std::cos(angle), std::sin(angle)} * upSpeed + remnant.vel * 0.3;
        jets.push_back(jp);
    }
}

void Simulation::updateParticles(double dt) {
    for (auto& p : particles) {
        Vec2 acc{0, 0};
        for (auto& bh : blackHoles) {
            if (bh.merged && bh.mergeProgress >= 1.0) continue;
            Vec2 diff = bh.pos - p.pos;
            double r2 = diff.len2();
            if (r2 < 0.1) { p.lifetime = 0; break; }
            double r = std::sqrt(r2);
            double F = config.G * bh.mass / (r2 + 0.5);
            acc = acc + (diff / r) * F;
        }
        p.vel = p.vel + acc * dt;
        p.pos = p.pos + p.vel * dt;
        p.lifetime -= dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& p) { return p.lifetime <= 0; }), particles.end());
}

void Simulation::updateKilonova(double dt) {
    for (auto& kp : kilonova) {
        kp.pos = kp.pos + kp.vel * dt;
        kp.vel = kp.vel * 0.98; // slight drag
        kp.lifetime -= dt;
    }
    kilonova.erase(std::remove_if(kilonova.begin(), kilonova.end(),
        [](const KilonovaParticle& kp) { return kp.lifetime <= 0; }), kilonova.end());
}

void Simulation::updateJets(double dt) {
    for (auto& jp : jets) {
        jp.pos = jp.pos + jp.vel * dt;
        jp.vel = jp.vel * 0.995;
        jp.lifetime -= dt;
    }
    jets.erase(std::remove_if(jets.begin(), jets.end(),
        [](const JetParticle& jp) { return jp.lifetime <= 0; }), jets.end());
}

void Simulation::spawnParticles() {
    if (particles.size() >= (size_t)config.maxParticles) return;
    for (auto& bh : blackHoles) {
        if (bh.merged && bh.mergeProgress >= 1.0) continue;
        if (std::rand() % 3 != 0) continue;

        double angle = randDouble() * 6.2832;
        double dist = bh.radius * (1.5 + randDouble() * 2.5);
        double speed = std::sqrt(config.G * bh.mass / dist) * 0.6;

        Particle p;
        p.pos = bh.pos + Vec2{dist * std::cos(angle), dist * std::sin(angle)};
        p.vel = Vec2{-speed * std::sin(angle), speed * std::cos(angle)} + bh.vel * 0.3;
        p.maxLife = 2.0 + randDouble() * 3.0;
        p.lifetime = p.maxLife;

        int ci = std::rand() % 5;
        if (ci < 2)      { p.r = 255; p.g = 100; p.b = 50; }
        else if (ci < 4) { p.r = 255; p.g = 200; p.b = 100; }
        else             { p.r = 100; p.g = 150; p.b = 255; }
        p.a = 200;
        particles.push_back(p);
    }
}

void Simulation::addTrailPoints() {
    for (auto& bh : blackHoles) {
        if (bh.merged) continue;
        bh.trail.push_back(bh.pos);
        if ((int)bh.trail.size() > BlackHole::MAX_TRAIL)
            bh.trail.erase(bh.trail.begin());
    }
}

void Simulation::updateGravWaves() {
    for (auto& gw : gravWaves) {
        double age = time - gw.startTime;
        if (age > 4.0) { gw.amplitude = 0; continue; }
        gw.amplitude = std::exp(-age * 0.8);
    }
    gravWaves.erase(std::remove_if(gravWaves.begin(), gravWaves.end(),
        [](const GravWave& gw) { return gw.amplitude <= 0; }), gravWaves.end());
}

void Simulation::updateGrid() {
    for (auto& gp : grid) {
        Vec2 displacement{0, 0};
        Vec2 pos = gp.origPos;
        for (auto& bh : blackHoles) {
            if (bh.merged && bh.mergeProgress >= 1.0) continue;
            Vec2 diff = pos - bh.pos;
            double r2 = diff.len2();
            if (r2 < 1.0) r2 = 1.0;
            double r = std::sqrt(r2);
            double strength = bh.mass / (r2 + 1.0);
            displacement = displacement + (diff / r) * strength;
        }
        gp.distortPos = pos + displacement;
    }
}

void Simulation::recordFrequency() {
    double freq = getOrbitalFrequency();
    if (freq > maxOrbitFreq) maxOrbitFreq = freq;
    freqHistory.push_back({time, freq});
    if (freqHistory.size() > 300) freqHistory.erase(freqHistory.begin());
}

double Simulation::getOrbitalFrequency() const {
    if (blackHoles.size() < 2) return 0;
    if (merged) {
        double ringAge = time - mergerTime;
        if (ringAge < 0.5) return 300.0 * std::exp(-ringAge * 3.0);
        return 0;
    }

    double minDist = 1e9;
    for (size_t i = 0; i < blackHoles.size(); i++)
        for (size_t j = i + 1; j < blackHoles.size(); j++)
            if (!blackHoles[i].merged && !blackHoles[j].merged)
                minDist = std::min(minDist, (blackHoles[i].pos - blackHoles[j].pos).len());

    if (minDist < 1.0) minDist = 1.0;
    double freq = 80.0 + 220.0 / std::sqrt(minDist);
    return clamp(freq, 20.0, 600.0);
}
