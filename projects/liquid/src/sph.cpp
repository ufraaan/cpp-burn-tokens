#include "sph.hpp"
#include <cstring>
#include <algorithm>

SPHSolver::SPHSolver(float worldW, float worldH, SPHParams params)
    : m_params(params), m_worldW(worldW), m_worldH(worldH)
{
    m_gridW = (int)(m_worldW / m_params.h) + 2;
    m_gridH = (int)(m_worldH / m_params.h) + 2;
    m_grid.resize(m_gridW * m_gridH);
}

void SPHSolver::addParticle(float x, float y, float vx, float vy) {
    Particle p;
    p.pos = {x, y};
    p.vel = {vx, vy};
    m_particles.push_back(p);
}

void SPHSolver::addBlock(int x, int y, int count) {
    for (int i = 0; i < count; i++)
        for (int j = 0; j < count; j++)
            addParticle(x + i * m_params.particleRadius * 2.5f,
                        y + j * m_params.particleRadius * 2.5f);
}

void SPHSolver::reset() {
    m_particles.clear();
}

void SPHSolver::buildGrid() {
    for (auto& cell : m_grid) cell.indices.clear();
    for (int i = 0; i < (int)m_particles.size(); i++) {
        int gx = (int)(m_particles[i].pos.x / m_params.h);
        int gy = (int)(m_particles[i].pos.y / m_params.h);
        gx = std::max(0, std::min(m_gridW - 1, gx));
        gy = std::max(0, std::min(m_gridH - 1, gy));
        m_grid[gy * m_gridW + gx].indices.push_back(i);
    }
}

float SPHSolver::poly6(float r2) const {
    if (r2 > m_params.h2 || r2 < 0) return 0;
    float diff = m_params.h2 - r2;
    return m_params.poly6Const * diff * diff * diff;
}

Vec2 SPHSolver::spikyGrad(Vec2 dir, float r) const {
    float diff = m_params.h - r;
    float val = m_params.spikyGradConst * diff * diff;
    return dir * val;
}

float SPHSolver::viscLap(float r) const {
    float diff = m_params.h - r;
    return m_params.viscLapConst * diff;
}

void SPHSolver::computeDensityPressure() {
    for (auto& p : m_particles) {
        p.density = 0;
        p.pressure = 0;
    }

    for (int i = 0; i < (int)m_particles.size(); i++) {
        Particle& pi = m_particles[i];
        int gx = (int)(pi.pos.x / m_params.h);
        int gy = (int)(pi.pos.y / m_params.h);

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                int ngx = gx + dx;
                int ngy = gy + dy;
                if (ngx < 0 || ngx >= m_gridW || ngy < 0 || ngy >= m_gridH) continue;
                for (int j : m_grid[ngy * m_gridW + ngx].indices) {
                    if (j <= i) continue;
                    Vec2 d = m_particles[j].pos - pi.pos;
                    float r2 = d.len2();
                    float w = poly6(r2);
                    pi.density += m_params.mass * w;
                    m_particles[j].density += m_params.mass * w;
                }
            }
        }
        pi.density += poly6(0) * m_params.mass; // self-contribution
        pi.pressure = m_params.gasConst * (pi.density - m_params.restDensity);
        if (pi.pressure < 0) pi.pressure = 0;
    }
}

void SPHSolver::computeForces() {
    m_forces.assign(m_particles.size(), Vec2{0, 0});

    for (int i = 0; i < (int)m_particles.size(); i++) {
        Particle& pi = m_particles[i];
        int gx = (int)(pi.pos.x / m_params.h);
        int gy = (int)(pi.pos.y / m_params.h);

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                int ngx = gx + dx;
                int ngy = gy + dy;
                if (ngx < 0 || ngx >= m_gridW || ngy < 0 || ngy >= m_gridH) continue;
                for (int j : m_grid[ngy * m_gridW + ngx].indices) {
                    if (j == i) continue;
                    Particle& pj = m_particles[j];
                    Vec2 d = pj.pos - pi.pos;
                    float r = d.len();
                    if (r < 0.001f || r > m_params.h) continue;
                    Vec2 dir = d / r;

                    // Pressure force
                    float avgP = (pi.pressure + pj.pressure) * 0.5f;
                    Vec2 fp = spikyGrad(dir, r) * (avgP / (pj.density * pi.density)) * m_params.mass;

                    // Viscosity force
                    Vec2 dv = pj.vel - pi.vel;
                    Vec2 fv = dv * (viscLap(r) * m_params.mass / (pi.density * pj.density) * m_params.viscosity);

                    // Surface tension (cohesion)
                    float cohesion = poly6(r * r) * m_params.surfaceTension;

                    m_forces[i] += fp + fv - dir * cohesion;
                    m_forces[j] -= fp + fv - dir * cohesion;
                }
            }
        }
    }
}

void SPHSolver::integrate(float dt) {
    Vec2 grav(0, m_params.gravity);
    for (int i = 0; i < (int)m_particles.size(); i++) {
        Vec2 accel = m_forces[i] + grav;
        m_particles[i].vel += accel * dt;
        m_particles[i].pos += m_particles[i].vel * dt;
    }
}

void SPHSolver::enforceBounds() {
    float r = m_params.particleRadius;
    float damp = m_params.wallDamping;
    float friction = m_params.wallFriction;
    float w = m_worldW, h = m_worldH;

    for (auto& p : m_particles) {
        if (p.pos.x < r) {
            p.pos.x = r;
            p.vel.x = -p.vel.x * damp;
            p.vel.y *= (1 - friction);
        }
        if (p.pos.x > w - r) {
            p.pos.x = w - r;
            p.vel.x = -p.vel.x * damp;
            p.vel.y *= (1 - friction);
        }
        if (p.pos.y < r) {
            p.pos.y = r;
            p.vel.y = -p.vel.y * damp;
            p.vel.x *= (1 - friction);
        }
        if (p.pos.y > h - r) {
            p.pos.y = h - r;
            p.vel.y = -p.vel.y * damp;
            p.vel.x *= (1 - friction);
        }
    }
}

void SPHSolver::update() {
    float subDt = m_params.dt / m_params.subSteps;
    for (int step = 0; step < m_params.subSteps; step++) {
        buildGrid();
        computeDensityPressure();
        computeForces();
        integrate(subDt);
        enforceBounds();
    }
}
