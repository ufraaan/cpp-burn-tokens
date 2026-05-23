#pragma once
#include <cmath>
#include <random>
#include <vector>

class PerlinNoise {
    std::vector<int> p;
public:
    PerlinNoise(unsigned seed = 42) {
        p.resize(512);
        std::vector<int> perm(256);
        for (int i = 0; i < 256; i++) perm[i] = i;
        std::shuffle(perm.begin(), perm.end(), std::default_random_engine(seed));
        for (int i = 0; i < 512; i++) p[i] = perm[i & 255];
    }

    double fade(double t) const { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
    double lerp(double a, double b, double t) const { return a + t * (b - a); }
    double grad(int hash, double x, double y) const {
        int h = hash & 3;
        double u = h < 2 ? x : y;
        double v = h < 2 ? y : x;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    double noise(double x, double y) const {
        int xi = (int)std::floor(x) & 255;
        int yi = (int)std::floor(y) & 255;
        double xf = x - std::floor(x);
        double yf = y - std::floor(y);
        double u = fade(xf);
        double v = fade(yf);
        int aa = p[p[xi] + yi];
        int ab = p[p[xi] + yi + 1];
        int ba = p[p[xi + 1] + yi];
        int bb = p[p[xi + 1] + yi + 1];
        double x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u);
        double x2 = lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u);
        return lerp(x1, x2, v);
    }

    double fbm(double x, double y, int octaves = 6) const {
        double value = 0, amplitude = 1, frequency = 1, maxVal = 0;
        for (int i = 0; i < octaves; i++) {
            value += amplitude * noise(x * frequency, y * frequency);
            maxVal += amplitude;
            amplitude *= 0.5;
            frequency *= 2.0;
        }
        return value / maxVal;
    }
};
