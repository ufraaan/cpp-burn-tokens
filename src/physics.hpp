#pragma once
#include "types.hpp"
#include <vector>
#include <cstdlib>

class Simulation {
public:
    Simulation();
    void update(double dt);
    void reset();
    void addBlackHole(Vec2 pos, Vec2 vel, double mass, double radius);

    // State
    std::vector<BlackHole> blackHoles;
    std::vector<Star> stars;
    std::vector<Particle> particles;
    std::vector<GridPoint> grid;
    std::vector<GravWave> gravWaves;
    std::vector<KilonovaParticle> kilonova;
    std::vector<JetParticle> jets;
    std::vector<FreqRecord> freqHistory;

    double time = 0.0;
    double speed = 1.0;
    bool paused = false;
    bool merged = false;
    double mergerTime = 0.0;
    Vec2 mergerPos;
    double maxOrbitFreq = 0.0;
    double shakeIntensity = 0.0;

    double getOrbitalFrequency() const;

    bool showGrid = true;
    bool showSpectrogram = true;

    SimulationConfig config;

private:
    void updateBlackHoles(double dt);
    void checkMerger();
    void updateParticles(double dt);
    void updateKilonova(double dt);
    void updateJets(double dt);
    void spawnParticles();
    void spawnKilonova();
    void spawnJets(double dt);
    void initStars();
    void initGrid();
    void updateGrid();
    void addTrailPoints();
    void updateGravWaves();
    void recordFrequency();
};
