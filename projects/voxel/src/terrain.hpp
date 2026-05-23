#pragma once
#include <vector>
#define GL_SILENCE_DEPRECATION
#include "noise.hpp"

struct Vertex {
    float x, y, z;
    float r, g, b;
};

struct Face {
    unsigned int a, b, c;
};

class Terrain {
public:
    int gridSize;
    float spacing;
    float heightScale;

    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    Terrain(int gridSize = 128, float spacing = 0.08f, float heightScale = 1.2f);
    void generate(unsigned seed);
    void setColor(float h, float& r, float& g, float& b) const;
    void render() const;
    void renderWireframe() const;

private:
    PerlinNoise perlin;
};
