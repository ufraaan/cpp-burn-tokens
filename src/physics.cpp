#include "physics.hpp"
#include <algorithm>
#include <cmath>

static double randDouble() {
    return (double)std::rand() / RAND_MAX;
}

Simulation::Simulation() {
    initStars();
    initGrid();
    reset();
}

void Simulation::reset() {
    blackHoles.clear();
    particles.clear();
    gravWaves.clear();
    merged = false;
    mergerTime = 0.0;
    mergerPos = {0, 0};
    maxOrbitFreq = 0.0;

    blackHoles.push_back({{-15.0, 0.0}, {0.0, -12.0}, 10.0, 3.0});
    blackHoles.push_back({{15.0, 0.0}, {0.0, 12.0}, 11.0, 3.2});
    time = 0.0;
}

void Simulation::addBlackHole(Vec2 pos, Vec2 vel, double mass, double radius) {
    blackHoles.push_back({pos, vel, mass, radius});
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
    for (int gx = -halfExt; gx <= halfExt; gx += spacing) {
        for (int gy = -halfExt; gy <= halfExt; gy += spacing) {
            Vec2 p{(double)gx, (double)gy};
            grid.push_back({p, p});
        }
    }
}

void Simulation::update(double dt) {
    if (paused) return;
    dt *= speed;

    for (int s = 0; s < config.substeps; s++) {
        updateBlackHoles(dt / config.substeps);
        checkMerger();
        updateParticles(dt / config.substeps);
        spawnParticles();
        addTrailPoints();
    }
    updateGravWaves();
    updateGrid();
    time += dt;
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

                // Create gravitational waves
                for (int w = 0; w < 5; w++) {
                    gravWaves.push_back({mergerPos, time + w * 0.05, 1.0});
                }

                a.pos = com;
                a.vel = comVel;
                a.mass = totalMass;
                a.radius = remnantRadius;
                a.merged = true;

                b.merged = true;
                b.mergeProgress = 1.0;
                break;
            }
        }
        if (merged) break;
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
        if ((int)bh.trail.size() > BlackHole::MAX_TRAIL) {
            bh.trail.erase(bh.trail.begin());
        }
    }
}

void Simulation::updateGravWaves() {
    for (auto& gw : gravWaves) {
        double age = time - gw.startTime;
        double lifetime = 4.0;
        if (age > lifetime) { gw.amplitude = 0; continue; }
        // Amplitude decays exponentially
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

double Simulation::getOrbitalFrequency() const {
    if (blackHoles.size() < 2) return 0;
    if (merged) {
        double ringAge = time - mergerTime;
        if (ringAge < 0.5) {
            return 300.0 * std::exp(-ringAge * 3.0);
        }
        return 0;
    }

    // Compute approximate orbital frequency from the closest pair
    double minDist = 1e9;
    for (size_t i = 0; i < blackHoles.size(); i++) {
        for (size_t j = i + 1; j < blackHoles.size(); j++) {
            if (blackHoles[i].merged || blackHoles[j].merged) continue;
            double d = (blackHoles[i].pos - blackHoles[j].pos).len();
            if (d < minDist) minDist = d;
        }
    }

    if (minDist < 1.0) minDist = 1.0;
    // Frequency increases as distance decreases (Kepler)
    double freq = 80.0 + 220.0 / std::sqrt(minDist);
    return clamp(freq, 20.0, 600.0);
}
