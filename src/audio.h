// Belt Command - synthesized sound effects and an ambient music loop, mixed in an SDL audio callback.
#pragma once
#include <SDL.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include "gfx.h"

enum Snd { SND_LASER, SND_MISSILE, SND_BOOM_S, SND_BOOM_L, SND_PLACE, SND_DONE, SND_ERR, SND_UI, SND_UI_OK,
           SND_ALERT, SND_ALIEN, SND_PING, SND_UPGRADE, SND_SHIELD, SND_WIN, SND_LOSE, SND_RECYCLE, SND_HIT, SND_N };

struct Audio {
    static const int RATE = 44100;
    SDL_AudioDeviceID dev = 0;
    std::vector<float> snd[SND_N];
    std::vector<float> music;
    struct Inst { int s; double pos; float rate, vol, pan; bool loop; };
    std::vector<Inst> inst;
    float sfxVol = 0.8f, musVol = 0.5f;
    Uint32 lastPlay[SND_N] = {};
    bool musicOn = false;

    static float sq(float ph) { return std::sin(ph) >= 0 ? 1.f : -1.f; }
    std::vector<float> gen(float dur, float (*f)(float t, float d, Rng& r, float& st)) {
        int n = (int)(dur * RATE); std::vector<float> v(n); Rng r; r.s = 99; float st = 0;
        for (int i = 0; i < n; i++) v[i] = f(i / (float)RATE, dur, r, st);
        return v;
    }
    void build() {
        snd[SND_LASER] = gen(0.13f, [](float t, float, Rng&, float& ph) { float f = 1700 * std::exp(-t * 11) + 380; ph += 2 * PI * f / RATE; return (0.5f * sq(ph) + 0.5f * std::sin(ph * 0.5f)) * std::exp(-t * 22) * 0.35f; });
        snd[SND_MISSILE] = gen(0.45f, [](float t, float, Rng& r, float& lp) { float n = r.range(-1, 1); lp += (n - lp) * 0.08f; float tone = std::sin(2 * PI * (180 + 500 * t) * t); return (lp * 1.6f + tone * 0.25f) * std::exp(-t * 6) * std::min(1.f, t * 60) * 0.5f; });
        snd[SND_BOOM_S] = gen(0.6f, [](float t, float, Rng& r, float& lp) { float k = 0.25f * std::exp(-t * 5) + 0.02f; lp += (r.range(-1, 1) - lp) * k; return lp * 2.2f * std::exp(-t * 6.5f) * 0.6f; });
        snd[SND_BOOM_L] = gen(1.6f, [](float t, float, Rng& r, float& lp) { float k = 0.12f * std::exp(-t * 3) + 0.01f; lp += (r.range(-1, 1) - lp) * k; float sub = std::sin(2 * PI * (55 - 15 * t) * t) * std::exp(-t * 2.5f); return (lp * 3.0f * std::exp(-t * 2.6f) + sub * 0.7f) * 0.55f; });
        snd[SND_PLACE] = gen(0.12f, [](float t, float, Rng&, float&) { float f = t < 0.05f ? 620.f : 930.f; return sq(2 * PI * f * t) * 0.18f * (t < 0.05f ? 1 : std::exp(-(t - 0.05f) * 30)); });
        snd[SND_DONE] = gen(0.3f, [](float t, float, Rng&, float&) { float f = t < 0.08f ? 523.f : t < 0.16f ? 659.f : 784.f; float lt = std::fmod(t, 0.08f); return std::sin(2 * PI * f * t) * 0.3f * std::exp(-lt * 18) * (t > 0.16f ? std::exp(-(t - 0.16f) * 10) : 1); });
        snd[SND_ERR] = gen(0.18f, [](float t, float, Rng&, float&) { return (sq(2 * PI * 150 * t) * 0.5f + sq(2 * PI * 157 * t) * 0.5f) * 0.18f * std::exp(-t * 8); });
        snd[SND_UI] = gen(0.04f, [](float t, float, Rng&, float&) { return std::sin(2 * PI * 1300 * t) * 0.22f * std::exp(-t * 70); });
        snd[SND_UI_OK] = gen(0.14f, [](float t, float, Rng&, float&) { float f = t < 0.05f ? 880.f : 1320.f; return std::sin(2 * PI * f * t) * 0.28f * std::exp(-std::fmod(t, 0.05f) * 25); });
        snd[SND_ALERT] = gen(0.9f, [](float t, float, Rng&, float&) { float f = std::fmod(t, 0.3f) < 0.15f ? 760.f : 540.f; return (sq(2 * PI * f * t) * 0.3f + std::sin(2 * PI * f * t) * 0.7f) * 0.2f * std::min(1.f, (0.9f - t) * 8); });
        snd[SND_ALIEN] = gen(0.11f, [](float t, float, Rng&, float& ph) { float f = 950 * std::exp(-t * 14) + 220; ph += 2 * PI * f / RATE; float saw = std::fmod(ph / (2 * PI), 1.f) * 2 - 1; return saw * std::exp(-t * 25) * 0.16f; });
        snd[SND_PING] = gen(0.5f, [](float t, float, Rng&, float&) { return (std::sin(2 * PI * 1480 * t) + 0.4f * std::sin(2 * PI * 2960 * t)) * 0.2f * std::exp(-t * 7); });
        snd[SND_UPGRADE] = gen(0.4f, [](float t, float, Rng&, float& ph) { float f = 400 + 1400 * t / 0.4f; ph += 2 * PI * f / RATE; return std::sin(ph) * 0.25f * std::min(1.f, (0.4f - t) * 10); });
        snd[SND_SHIELD] = gen(0.16f, [](float t, float, Rng& r, float& lp) { float n = r.range(-1, 1); float hp = n - lp; lp = n; return (hp * 0.3f + std::sin(2 * PI * 2200 * t) * 0.2f) * std::exp(-t * 18) * 0.5f; });
        snd[SND_WIN] = gen(1.4f, [](float t, float, Rng&, float&) { const float nf[5] = { 523, 659, 784, 1046, 1318 }; int i = std::min(4, (int)(t / 0.14f)); float lt = t - i * 0.14f; float v = 0; for (int k = 0; k <= i; k++) v += std::sin(2 * PI * nf[k] * t) * std::exp(-(t - k * 0.14f) * 2.2f); return v * 0.12f * std::exp(-lt * 0.5f); });
        snd[SND_LOSE] = gen(1.6f, [](float t, float, Rng&, float& ph) { float f = 330 * std::exp(-t * 0.9f); ph += 2 * PI * f / RATE; return (std::sin(ph) + 0.3f * sq(ph)) * 0.2f * std::exp(-t * 1.6f); });
        snd[SND_RECYCLE] = gen(0.3f, [](float t, float, Rng&, float& ph) { float f = 1200 - 2600 * t; ph += 2 * PI * std::max(f, 200.f) / RATE; return std::sin(ph) * 0.22f * std::exp(-t * 6); });
        snd[SND_HIT] = gen(0.07f, [](float t, float, Rng& r, float&) { return r.range(-1, 1) * 0.25f * std::exp(-t * 60); });
        buildMusic();
    }
    // 48 s ambient loop: slow minor pads plus sparse echoing plucks.
    void buildMusic() {
        const float secs = 48; int n = (int)(secs * RATE); music.assign(n * 2, 0.f);
        const float chords[4][4] = { { 110.0f, 164.81f, 220.0f, 261.63f }, { 87.31f, 130.81f, 174.61f, 220.0f }, { 130.81f, 196.0f, 261.63f, 329.63f }, { 98.0f, 146.83f, 196.0f, 246.94f } };
        for (int i = 0; i < n; i++) {
            float t = i / (float)RATE; int c = (int)(t / 12) % 4; float lt = std::fmod(t, 12.f);
            float env = std::min(1.f, lt / 3.f) * std::min(1.f, (12 - lt) / 3.f);
            int cn = (c + 1) % 4; float xf = lt > 9 ? (lt - 9) / 3 : 0;
            float l = 0, r = 0;
            for (int k = 0; k < 4; k++) {
                float a = chords[c][k], b = chords[cn][k];
                float va = std::sin(2 * PI * a * t) + 0.3f * std::sin(2 * PI * a * 2.003f * t);
                float vb = std::sin(2 * PI * b * t) + 0.3f * std::sin(2 * PI * b * 2.003f * t);
                float lfo = 0.5f + 0.5f * std::sin(t * (0.21f + k * 0.07f));
                float v = (va * (1 - xf) + vb * xf) * (0.6f + 0.4f * lfo);
                l += v * (k % 2 ? 0.8f : 1.0f); r += v * (k % 2 ? 1.0f : 0.8f);
            }
            music[i * 2] = l * 0.045f; music[i * 2 + 1] = r * 0.045f;
        }
        // plucks
        Rng rg; rg.s = 4242; float t = 1.0f;
        while (t < secs - 1) {
            int c = (int)(t / 12) % 4; const float* ch = chords[c];
            float f = ch[rg.i(4)] * (rg.f() < 0.5f ? 4 : 2);
            int st = (int)(t * RATE), len = (int)(1.2f * RATE); float pan = rg.range(-0.6f, 0.6f);
            for (int j = 0; j < len; j++) {
                float tt = j / (float)RATE; float v = std::sin(2 * PI * f * tt) * std::exp(-tt * 4.5f) * 0.05f;
                int idx = (st + j) % n; music[idx * 2] += v * (1 - pan); music[idx * 2 + 1] += v * (1 + pan);
            }
            t += rg.f() < 0.3f ? 0.375f : rg.range(0.75f, 2.25f);
        }
        // echo (two passes so the loop wraps cleanly)
        int dl = (int)(0.375f * 2 * RATE);
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < n; i++) { int j = (i + dl) % n; music[j * 2] += music[i * 2 + 1] * 0.28f; music[j * 2 + 1] += music[i * 2] * 0.28f; }
        float pk = 0; for (float v : music) pk = std::max(pk, std::fabs(v));
        if (pk > 0.6f) for (float& v : music) v *= 0.6f / pk;
    }
    static void cb(void* ud, Uint8* stream, int bytes) {
        Audio* a = (Audio*)ud; float* out = (float*)stream; int frames = bytes / 8;
        for (int i = 0; i < frames * 2; i++) out[i] = 0;
        if (a->musicOn && !a->music.empty()) {
            static double mpos = 0; int mn = (int)a->music.size() / 2;
            for (int i = 0; i < frames; i++) { int p = (int)mpos; out[i * 2] += a->music[p * 2] * a->musVol; out[i * 2 + 1] += a->music[p * 2 + 1] * a->musVol; mpos += 1; if (mpos >= mn) mpos -= mn; }
        }
        for (size_t k = 0; k < a->inst.size(); ) {
            Inst& in = a->inst[k]; const std::vector<float>& s = a->snd[in.s];
            float vl = in.vol * a->sfxVol * (1 - std::max(0.f, in.pan)), vr = in.vol * a->sfxVol * (1 + std::min(0.f, in.pan));
            bool done = false;
            for (int i = 0; i < frames; i++) {
                int p = (int)in.pos; if (p >= (int)s.size()) { done = true; break; }
                float v = s[p]; out[i * 2] += v * vl; out[i * 2 + 1] += v * vr; in.pos += in.rate;
            }
            if (done) { a->inst[k] = a->inst.back(); a->inst.pop_back(); } else k++;
        }
        for (int i = 0; i < frames * 2; i++) { float v = out[i]; out[i] = v / (1 + std::fabs(v) * 0.6f); }
    }
    bool open() {
        build();
        SDL_AudioSpec want = {}, have = {};
        want.freq = RATE; want.format = AUDIO_F32SYS; want.channels = 2; want.samples =
#ifdef __EMSCRIPTEN__
            2048; // browsers run the audio callback on the main thread; a bigger buffer avoids crackles
#else
            1024;
#endif
        want.callback = cb; want.userdata = this;
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!dev) { printf("audio: open failed: %s\n", SDL_GetError()); return false; }
        printf("audio: %d Hz, %d ch\n", have.freq, have.channels);
        musicOn = true;
        SDL_PauseAudioDevice(dev, 0);
        return true;
    }
    void play(int s, float vol = 1, float pan = 0, float pitch = 1, Uint32 minGapMs = 30) {
        if (!dev || vol <= 0.01f) return;
        Uint32 now = SDL_GetTicks(); if (now - lastPlay[s] < minGapMs) return; lastPlay[s] = now;
        SDL_LockAudioDevice(dev);
        if (inst.size() < 48) inst.push_back({ s, 0.0, pitch, vol, clampf(pan, -1, 1), false });
        SDL_UnlockAudioDevice(dev);
    }
    void close() { if (dev) SDL_CloseAudioDevice(dev); dev = 0; }
};
