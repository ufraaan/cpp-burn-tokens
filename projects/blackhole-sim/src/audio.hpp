#pragma once
#include <SDL2/SDL.h>
#include <atomic>
#include <cmath>

struct AudioData {
    std::atomic<double> frequency{80.0};
    std::atomic<double> amplitude{0.0};
    double phase = 0.0;
};

class Audio {
public:
    Audio();
    ~Audio();
    void setFrequency(double freq);
    void setAmplitude(double amp);
    bool isInitialized() const { return ok; }
private:
    static void callback(void* userdata, Uint8* stream, int len);
    SDL_AudioDeviceID dev = 0;
    AudioData data;
    bool ok = false;
};
