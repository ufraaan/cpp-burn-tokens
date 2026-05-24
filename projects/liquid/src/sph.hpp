#pragma once
#include "types.hpp"
#include <vector>
#include <cmath>

struct SPHParams {
    float h = 20.0f;
    float h2 = h * h;
    float restDensity = 1.0f;
    float gasConst = 8.0f;
    float viscosity = 0.12f;
    float gravity = 350.0f;
    float dt = 0.0025f;
    float particleRadius = 4.0f;
    float mass = 1.0f;
    float wallDamping = 0.4f;
    float wallFriction = 0.1f;
    int subSteps = 4;
    float surfaceTension = 0.5f;

    float poly6Const;
    float spikyGradConst;
    float viscLapConst;

    SPHParams() {
        poly6Const = 315.0f / (64.0f * M_PI * std::pow(h, 9));
        spikyGradConst = -45.0f / (M_PI * std::pow(h, 6));
        viscLapConst = 45.0f / (M_PI * std::pow(h, 6));
    }
};

class SPHSolver {
public:
    SPHSolver(float worldW, float worldH, SPHParams params = SPHParams());

    void update();
    void addParticle(float x, float y, float vx = 0, float vy = 0);
    void addBlock(int x, int y, int count);
    void reset();

    const std::vector<Particle>& particles() const { return m_particles; }
    const SPHParams& params() const { return m_params; }
    float worldW() const { return m_worldW; }
    float worldH() const { return m_worldH; }

private:
    void computeDensityPressure();
    void computeForces();
    void integrate(float dt);
    void enforceBounds();
    void buildGrid();

    float poly6(float r2) const;
    Vec2 spikyGrad(Vec2 dir, float r) const;
    float viscLap(float r) const;

    std::vector<Particle> m_particles;
    std::vector<Vec2> m_forces;
    SPHParams m_params;
    float m_worldW, m_worldH;

    struct GridCell {
        std::vector<int> indices;
    };
    int m_gridW, m_gridH;
    std::vector<GridCell> m_grid;
};
