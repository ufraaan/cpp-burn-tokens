#include "terrain.hpp"
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <cmath>

Terrain::Terrain(int gs, float sp, float hs)
    : gridSize(gs), spacing(sp), heightScale(hs), perlin(42) {}

void Terrain::generate(unsigned seed) {
    vertices.clear();
    faces.clear();
    perlin = PerlinNoise(seed);

    float half = (gridSize - 1) * spacing * 0.5f;

    // Generate vertices
    for (int z = 0; z < gridSize; z++) {
        for (int x = 0; x < gridSize; x++) {
            float wx = x * spacing - half;
            float wz = z * spacing - half;

            double nx = (double)x / gridSize * 4.0;
            double nz = (double)z / gridSize * 4.0;
            double h = perlin.fbm(nx, nz, 7) * heightScale;

            Vertex v;
            v.x = wx;
            v.y = (float)h;
            v.z = wz;
            setColor((float)h, v.r, v.g, v.b);
            vertices.push_back(v);
        }
    }

    // Generate triangle strip indices as indexed faces
    for (int z = 0; z < gridSize - 1; z++) {
        for (int x = 0; x < gridSize - 1; x++) {
            unsigned int i0 = z * gridSize + x;
            unsigned int i1 = z * gridSize + x + 1;
            unsigned int i2 = (z + 1) * gridSize + x;
            unsigned int i3 = (z + 1) * gridSize + x + 1;
            faces.push_back({i0, i1, i2});
            faces.push_back({i1, i3, i2});
        }
    }
}

void Terrain::setColor(float h, float& r, float& g, float& b) const {
    // Height-based coloring: water -> sand -> grass -> forest -> rock -> snow
    if (h < -0.25f) {
        float t = (h + 0.25f) / 0.15f + 1.0f;
        t = (t < 0) ? 0 : (t > 1 ? 1 : t);
        r = 0.05f; g = 0.05f + 0.15f * t; b = 0.2f + 0.3f * t;
    } else if (h < -0.1f) {
        float t = (h + 0.25f) / 0.15f;
        r = 0.05f + 0.25f * t; g = 0.2f + 0.3f * t; b = 0.5f - 0.2f * t;
    } else if (h < 0.0f) {
        float t = (h + 0.1f) / 0.1f;
        r = 0.3f + 0.4f * t; g = 0.5f + 0.2f * t; b = 0.3f - 0.15f * t;
    } else if (h < 0.15f) {
        float t = h / 0.15f;
        r = 0.7f - 0.15f * t; g = 0.7f + 0.1f * t; b = 0.15f - 0.05f * t;
    } else if (h < 0.35f) {
        float t = (h - 0.15f) / 0.2f;
        r = 0.55f - 0.15f * t; g = 0.8f - 0.15f * t; b = 0.1f + 0.05f * t;
    } else if (h < 0.55f) {
        float t = (h - 0.35f) / 0.2f;
        r = 0.4f + 0.15f * t; g = 0.65f - 0.2f * t; b = 0.15f;
    } else {
        float t = (h - 0.55f) / 0.25f;
        if (t > 1) t = 1;
        r = 0.55f + 0.45f * t; g = 0.45f + 0.55f * t; b = 0.15f + 0.85f * t;
    }
}

void Terrain::render() const {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &vertices[0].x);
    glColorPointer(3, GL_FLOAT, sizeof(Vertex), &vertices[0].r);

    glDrawElements(GL_TRIANGLES, (GLsizei)(faces.size() * 3), GL_UNSIGNED_INT, &faces[0].a);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

void Terrain::renderWireframe() const {
    glDisable(GL_LIGHTING);
    glColor3f(0.3f, 0.4f, 0.5f);

    for (size_t i = 0; i < faces.size(); i++) {
        glBegin(GL_LINE_LOOP);
        glVertex3fv(&vertices[faces[i].a].x);
        glVertex3fv(&vertices[faces[i].b].x);
        glVertex3fv(&vertices[faces[i].c].x);
        glEnd();
    }

    glEnable(GL_LIGHTING);
}
