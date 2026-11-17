#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace ark {

struct OnePole {
    float z = 0.f;
    float a = 0.f;
    void setLP(float fc, double sr) { a = 1.f - std::exp(-6.2831853f * fc / (float)sr); }
    float lp(float x) { z += a * (x - z); return z; }
    float hp(float x) { z += a * (x - z); return x - z; }
};

struct Line {
    std::vector<float> b;
    int w = 0;
    int n = 0;
    void init(int len) { n = len + 4; b.assign(n, 0.f); w = 0; }
    void clear() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
    void push(float x) { b[w] = x; if (++w >= n) w = 0; }
    float read(float d) const {
        if (d < 1.f) d = 1.f;
        if (d > (float)(n - 3)) d = (float)(n - 3);
        int di = (int)d;
        float f = d - (float)di;
        int i = w - 1 - di;
        while (i < 0) i += n;
        int j = i - 1;
        if (j < 0) j += n;
        return b[i] + (b[j] - b[i]) * f;
    }
};

struct Allpass {
    std::vector<float> b;
    int w = 0;
    int n = 1;
    float g = 0.6f;
    void init(int len, float gain) { n = std::max(1, len); b.assign(n, 0.f); w = 0; g = gain; }
    void clear() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
    float run(float x) {
        float d = b[w];
        float v = x - g * d;
        b[w] = v;
        if (++w >= n) w = 0;
        return d + g * v;
    }
};

struct Ducker {
    float env = 0.f, gain = 1.f;
    float ea = 0.f, er = 0.f, gs = 0.f;
    void prepare(double sr)
    {
        ea = 1.f - std::exp(-1.f / (0.004f * (float)sr));
        er = 1.f - std::exp(-1.f / (0.25f * (float)sr));
        gs = 1.f - std::exp(-1.f / (0.03f * (float)sr));
        env = 0.f;
        gain = 1.f;
    }
    float step(float l, float r, float amount)
    {
        float a = std::fabs(0.5f * (l + r));
        env += (a > env ? ea : er) * (a - env);
        float vox = std::min(1.f, env * 6.f);
        gain += gs * ((1.f - amount * vox) - gain);
        return gain;
    }
};

struct ReverbParams {
    float mix = 0.3f;
    float dry = 1.f;
    float decay = 1.8f;
    float pre = 20.f;
    float damp = 7000.f;
    float lowCut = 180.f;
    float width = 1.f;
    float duck = 0.5f;
    float mod = 0.5f;
};

struct Reverb {
    static constexpr int N = 8;
    double sr = 44100.0;
    ReverbParams P;
    Line preL, preR;
    Allpass apL[4], apR[4];
    Line fd[N];
    OnePole dampLp[N];
    float baseLen[N] = {1087, 1283, 1481, 1699, 1913, 2111, 2293, 2477};
    float lfoPh[N];
    float lfoRate[N];
    float g[N];
    OnePole outHp[2], outLp[2];
    Ducker duck;
    float smMix = 0.f, smDry = 1.f, widthS = 1.f;

    void prepare(double s)
    {
        sr = s;
        preL.init((int)(0.25 * sr));
        preR.init((int)(0.25 * sr));
        static const int apLen[4] = {142, 107, 379, 277};
        static const int apRen[4] = {151, 113, 397, 263};
        double k = sr / 44100.0;
        for (int i = 0; i < 4; i++) {
            apL[i].init((int)(apLen[i] * k), 0.65f);
            apR[i].init((int)(apRen[i] * k), 0.65f);
        }
        for (int i = 0; i < N; i++) {
            fd[i].init((int)(baseLen[i] * k * 2.6) + 64);
            lfoPh[i] = (float)i / N;
            lfoRate[i] = 0.17f + 0.043f * i;
        }
        duck.prepare(sr);
        reset();
        update();
        smMix = P.mix;
        smDry = P.dry;
        widthS = P.width;
    }

    void reset()
    {
        preL.clear(); preR.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        for (int i = 0; i < N; i++) { fd[i].clear(); dampLp[i].z = 0.f; }
        for (int c = 0; c < 2; c++) { outHp[c].z = 0.f; outLp[c].z = 0.f; }
        duck.env = 0.f;
        duck.gain = 1.f;
    }

    void set(const ReverbParams& np)
    {
        P = np;
        update();
    }

    float lenOf(int i) const { return baseLen[i] * (float)(sr / 44100.0) * (0.6f + 0.5f * std::min(3.f, P.decay) / 3.f * 1.6f); }

    void update()
    {
        for (int i = 0; i < N; i++) {
            float rt = std::max(0.1f, P.decay) * 1.17f;
            g[i] = std::pow(10.f, -3.f * lenOf(i) / (rt * (float)sr));
            dampLp[i].setLP(P.damp, sr);
        }
        for (int c = 0; c < 2; c++) {
            outHp[c].setLP(std::max(20.f, P.lowCut), sr);
            outLp[c].setLP(std::min(18000.f, P.damp * 1.8f), sr);
        }
    }

    void process(const float* inL, const float* inR, float* oL, float* oR, int n)
    {
        const float sm = 1.f - std::exp(-1.f / (0.02f * (float)sr));
        const float preS = std::min((float)(0.24 * sr), P.pre * 0.001f * (float)sr);
        const float modAmt = 14.f * P.mod * (float)(sr / 44100.0);
        for (int s = 0; s < n; s++) {
            float l = inL[s], r = inR[s];
            smMix += sm * (P.mix - smMix);
            smDry += sm * (P.dry - smDry);
            widthS += sm * (P.width - widthS);
            float dg = duck.step(l, r, P.duck);
            preL.push(l);
            preR.push(r);
            float xl = preL.read(preS);
            float xr = preR.read(preS);
            for (int i = 0; i < 4; i++) { xl = apL[i].run(xl); xr = apR[i].run(xr); }
            float v[N];
            float sum = 0.f;
            for (int i = 0; i < N; i++) {
                lfoPh[i] += lfoRate[i] / (float)sr;
                if (lfoPh[i] > 1.f) lfoPh[i] -= 1.f;
                float mod = modAmt * 0.5f * std::sin(6.2831853f * lfoPh[i]);
                v[i] = fd[i].read(lenOf(i) + mod);
                v[i] = dampLp[i].lp(v[i]) * g[i];
                sum += v[i];
            }
            float k = 2.f * sum / (float)N;
            for (int i = 0; i < N; i++) fd[i].push(v[i] - k + ((i & 1) ? xr : xl) * 0.35f);
            float al = 0.f, ar = 0.f;
            for (int i = 0; i < N; i++) {
                float sg = (i & 2) ? -1.f : 1.f;
                if (i & 1) ar += v[i] * sg; else al += v[i] * sg;
            }
            al *= 0.5f; ar *= 0.5f;
            float mid = 0.5f * (al + ar), side = 0.5f * (al - ar);
            al = mid + side * widthS;
            ar = mid - side * widthS;
            al = outLp[0].lp(outHp[0].hp(al));
            ar = outLp[1].lp(outHp[1].hp(ar));
            oL[s] = l * smDry + al * 4.f * smMix * dg;
            oR[s] = r * smDry + ar * 4.f * smMix * dg;
        }
    }
};

struct DelayParams {
    float mix = 0.35f;
    float dry = 1.f;
    float timeMs = 300.f;
    float feed = 0.4f;
    float low = 250.f;
    float high = 5500.f;
    float ping = 1.f;
    float duck = 0.5f;
    float wobble = 0.f;
    float drive = 0.2f;
};

struct Delay {
    double sr = 44100.0;
    DelayParams P;
    Line dl[2];
    OnePole hp[2], lp[2];
    Ducker duck;
    float smMix = 0.f, smDry = 1.f, smT = 300.f, ph = 0.f;

    void prepare(double s)
    {
        sr = s;
        dl[0].init((int)(2.6 * sr));
        dl[1].init((int)(2.6 * sr));
        duck.prepare(sr);
        reset();
        update();
        smMix = P.mix;
        smDry = P.dry;
        smT = P.timeMs;
    }

    void reset()
    {
        dl[0].clear();
        dl[1].clear();
        for (int c = 0; c < 2; c++) { hp[c].z = 0.f; lp[c].z = 0.f; }
        duck.env = 0.f;
        duck.gain = 1.f;
    }

    void set(const DelayParams& np)
    {
        P = np;
        update();
    }

    void update()
    {
        for (int c = 0; c < 2; c++) {
            hp[c].setLP(std::max(20.f, P.low), sr);
            lp[c].setLP(std::min(18000.f, P.high), sr);
        }
    }

    void process(const float* inL, const float* inR, float* oL, float* oR, int n)
    {
        const float sm = 1.f - std::exp(-1.f / (0.02f * (float)sr));
        const float smTc = 1.f - std::exp(-1.f / (0.15f * (float)sr));
        const float drv = 1.f + 3.f * P.drive;
        const float wob = P.wobble * 0.0035f * (float)sr;
        for (int s = 0; s < n; s++) {
            float l = inL[s], r = inR[s];
            float m = 0.5f * (l + r);
            smMix += sm * (P.mix - smMix);
            smDry += sm * (P.dry - smDry);
            smT += smTc * (P.timeMs - smT);
            ph += 0.55f / (float)sr;
            if (ph > 1.f) ph -= 1.f;
            float dg = duck.step(l, r, P.duck);
            float dsm = smT * 0.001f * (float)sr;
            float tl = dl[0].read(dsm + wob * std::sin(6.2831853f * ph));
            float tr = dl[1].read(dsm + wob * std::sin(6.2831853f * ph + 1.7f));
            float fl = lp[0].lp(hp[0].hp(tl));
            float fr = lp[1].lp(hp[1].hp(tr));
            fl = std::tanh(fl * drv) / drv;
            fr = std::tanh(fr * drv) / drv;
            float ping = P.ping;
            float srcL = l * (1.f - ping) + m * ping;
            float srcR = r * (1.f - ping);
            dl[0].push(srcL + (fr * ping + fl * (1.f - ping)) * P.feed);
            dl[1].push(srcR + (fl * ping + fr * (1.f - ping)) * P.feed);
            oL[s] = l * smDry + tl * smMix * dg;
            oR[s] = r * smDry + tr * smMix * dg;
        }
    }
};

}
