#include "audio.hpp"
#include "types.hpp"
#include <cmath>
#include <algorithm>

Audio::Audio() {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = callback;
    want.userdata = &data;

    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        // Audio not available, that's OK
        return;
    }

    ok = true;
    SDL_PauseAudioDevice(dev, 0);
}

Audio::~Audio() {
    if (dev != 0) {
        SDL_CloseAudioDevice(dev);
    }
}

void Audio::setFrequency(double freq) {
    data.frequency.store(freq);
}

void Audio::setAmplitude(double amp) {
    data.amplitude.store(amp);
}

void Audio::callback(void* userdata, Uint8* stream, int len) {
    auto* ad = static_cast<AudioData*>(userdata);
    auto* samples = reinterpret_cast<Sint16*>(stream);
    int count = len / sizeof(Sint16);

    double freq = ad->frequency.load();
    double amp = ad->amplitude.load();

    if (amp < 0.001) {
        std::fill(samples, samples + count, 0);
        return;
    }

    for (int i = 0; i < count; i++) {
        double sample = amp * std::sin(ad->phase);
        samples[i] = static_cast<Sint16>(clamp(sample, -1.0, 1.0) * 32767);
        ad->phase += 2.0 * M_PI * freq / 44100.0;
        if (ad->phase > 2.0 * M_PI) ad->phase -= 2.0 * M_PI;
    }
}
