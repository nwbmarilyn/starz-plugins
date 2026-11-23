#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace ark {

struct AdaptiveEqParams {
    float amount = 0.5f;
};

class AdaptiveEq {
public:
    static constexpr int kBands = 6;
    static constexpr std::array<float, kBands> kFrequencies = {160.f, 320.f, 640.f, 1200.f, 2600.f, 4400.f};

    void prepare(double sampleRate)
    {
        const float sr = static_cast<float>(sampleRate);
        analysisInterval = std::max(1, static_cast<int>(std::lround(sr / 200.f)));
        powerSpeed = 1.f - std::exp(-1.f / (0.085f * sr));
        engageSpeed = 1.f - std::exp(-analysisInterval / (0.17f * sr));
        releaseSpeed = 1.f - std::exp(-analysisInterval / (0.37f * sr));
        gainSpeed = 1.f - std::exp(-1.f / (0.030f * sr));
        for (int i = 0; i < kBands; ++i)
            bands[i].filter.prepare(std::min(kFrequencies[i], 0.4f * sr), sr);
        reset();
    }

    void reset()
    {
        samplesLeft = analysisInterval;
        vocalPower = beatPower = 0.f;
        for (auto& band : bands) {
            band.vocal = {};
            band.beat = {};
            band.output = {};
            band.vocalPower = band.beatPower = 0.f;
            band.gainDb = band.target = band.amount = 0.f;
        }
    }

    void set(const AdaptiveEqParams& params)
    {
        amount = std::clamp(params.amount, 0.f, 1.f);
    }

    float gainDb(int band) const { return bands[band].gainDb; }

    void process(const float* vocalL, const float* vocalR, const float* beatL, const float* beatR,
                 float* outL, float* outR, int n)
    {
        for (int sample = 0; sample < n; ++sample) {
            float l = vocalL[sample];
            float r = vocalR ? vocalR[sample] : l;
            const float bl = beatL ? beatL[sample] : 0.f;
            const float br = beatR ? beatR[sample] : bl;
            vocalPower += powerSpeed * (power(l, r) - vocalPower);
            beatPower += powerSpeed * (power(bl, br) - beatPower);

            for (auto& band : bands) {
                const float vl = band.filter.run(l, band.vocal[0]);
                const float vr = band.filter.run(r, band.vocal[1]);
                const float kl = band.filter.run(bl, band.beat[0]);
                const float kr = band.filter.run(br, band.beat[1]);
                band.vocalPower += powerSpeed * (power(vl, vr) - band.vocalPower);
                band.beatPower += powerSpeed * (power(kl, kr) - band.beatPower);
            }
            if (--samplesLeft == 0) {
                update();
                samplesLeft = analysisInterval;
            }
            for (auto& band : bands) {
                band.amount += gainSpeed * (band.target - band.amount);
                l += band.amount * band.filter.run(l, band.output[0]);
                r += band.amount * band.filter.run(r, band.output[1]);
            }
            outL[sample] = l;
            if (outR) outR[sample] = r;
        }
    }

private:
    struct Filter {
        using State = std::array<float, 2>;
        static constexpr float k = 1.f / 1.45f;
        float a1 = 1.f, a2 = 0.f, a3 = 0.f;

        void prepare(float hz, float sr)
        {
            const float g = std::tan(3.141592653589793f * hz / sr);
            a1 = 1.f / (1.f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        float run(float x, State& state) const
        {
            const float hp = x - state[1];
            const float bp = a1 * state[0] + a2 * hp;
            const float lp = state[1] + a2 * state[0] + a3 * hp;
            state[0] = 2.f * bp - state[0] + 1e-20f;
            state[1] = 2.f * lp - state[1] + 1e-20f;
            return k * bp;
        }
    };

    struct Band {
        Filter filter;
        std::array<Filter::State, 2> vocal{}, beat{}, output{};
        float vocalPower = 0.f;
        float beatPower = 0.f;
        float gainDb = 0.f;
        float target = 0.f;
        float amount = 0.f;
    };

    std::array<Band, kBands> bands{};
    int analysisInterval = 240, samplesLeft = 240;
    float vocalPower = 0.f, beatPower = 0.f, amount = 0.5f;
    float powerSpeed = 0.f, engageSpeed = 0.f, releaseSpeed = 0.f, gainSpeed = 0.f;

    static float power(float l, float r) { return 0.5f * (l * l + r * r); }
    static float db(float power) { return 10.f * std::log10(std::max(1e-12f, power)); }
    static float unit(float value) { return std::clamp(value, 0.f, 1.f); }

    void update()
    {
        static constexpr std::array<float, kBands> maxCut = {2.f, 3.f, 2.5f, 1.f, 0.f, 0.f};
        static constexpr std::array<float, kBands> maxLift = {0.f, 0.f, 0.f, 0.f, 1.5f, 0.8f};
        const float vocal = db(vocalPower);
        const float beat = db(beatPower);
        const float activity = unit((vocal + 58.f) / 22.f) * unit((beat + 58.f) / 22.f);
        for (int i = 0; i < kBands; ++i) {
            Band& band = bands[i];
            const Band& prev = bands[std::max(0, i - 1)];
            const Band& next = bands[std::min(kBands - 1, i + 1)];
            const float vocalBand = db(band.vocalPower);
            const float beatBand = db(band.beatPower);
            const float localVocal = db(0.5f * (prev.vocalPower + next.vocalPower));
            const float prominence = unit((vocalBand - localVocal - 0.8f) / 2.f);
            const float overlap = unit((beatBand - beat + 23.f) / 14.f) * unit((beatBand - vocalBand + 16.f) / 14.f);
            const float vocalContent = unit((vocalBand - vocal + 28.f) / 18.f);
            const float space = unit((beat - beatBand - 7.f) / 16.f);
            const float need = unit((beat - vocal + 2.f) / 14.f);
            const float targetDb = amount * activity *
                (maxLift[i] * vocalContent * space * need - maxCut[i] * prominence * overlap);
            const float speed = std::fabs(targetDb) > std::fabs(band.gainDb) ? engageSpeed : releaseSpeed;
            band.gainDb += speed * (targetDb - band.gainDb);
            band.target = std::pow(10.f, band.gainDb / 20.f) - 1.f;
        }
    }
};

}
