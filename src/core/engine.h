#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ark {

inline float gGate = 0.003f;
inline float gThr = 0.15f;
inline float gFall = 0.35f;
inline float gHyst = 0.22f;
inline float gCont = 0.f;
inline float gMake = 0.30f;
inline float gShelf = 0.28f;
inline float gShelfHz = 5000.f;
inline int gHold = 3;
inline std::vector<std::pair<int64_t, float>>* gTrace = nullptr;
constexpr int kRing = 1 << 16;
constexpr int kMask = kRing - 1;
constexpr int kHop = 128;
constexpr int kMaxVoices = 24;
constexpr int kDetRing = 512;
constexpr int kMarkRing = 4096;

struct Params {
    float refHz = 436.f;
    int root = 7;
    int scaleType = 0;
    float retuneMs = 0.5f;
    bool tune = true;
    bool preSnap = true;
    int voices = 8;
    float spreadCents = 25.f;
    float driftCents = 40.f;
    float driftHz = 1.2f;
    float flangeMs = 7.f;
    float flangeDepthMs = 1.f;
    float delaySpreadMs = 12.f;
    float flangeRate = 0.f;
    float wet = 1.f;
    int stereoMode = 0;
    float minF0 = 100.f;
    float outGain = 1.f;
    uint32_t seed = 1;
    float swarm = 1.f;
    int solo = -1;
    float latK = 2.f;
};

inline Params presetKitty()
{
    Params p;
    p.voices = 8;
    p.spreadCents = 25.f;
    p.driftCents = 40.f;
    p.driftHz = 1.2f;
    p.retuneMs = 4.f;
    p.preSnap = true;
    p.flangeMs = 7.f;
    p.flangeDepthMs = 1.f;
    p.delaySpreadMs = 12.f;
    p.wet = 1.f;
    return p;
}

inline Params presetArtist()
{
    Params p;
    p.voices = 1;
    p.spreadCents = 0.f;
    p.driftCents = 0.f;
    p.retuneMs = 0.5f;
    p.preSnap = false;
    p.flangeMs = 0.f;
    p.flangeDepthMs = 0.f;
    p.delaySpreadMs = 0.f;
    p.wet = 1.f;
    return p;
}

struct Rng {
    uint32_t s = 2463534242u;
    explicit Rng(uint32_t seed = 1) { s = seed * 2654435761u + 88172645u; if (!s) s = 1; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * uni(); }
    float gauss() { return (uni() + uni() + uni() + uni() - 2.0f) * 1.7320508f; }
};

struct Voice {
    bool active = false;
    double ts = 0;
    double cs = 0;
    bool csInit = false;
    int64_t frontier = INT64_MAX;
    int64_t emitted = 0;
    float det = 0, driftScale = 1, driftHzScale = 1, driftState = 0;
    float flU1 = 1, flU2 = 0, flRate = 0.5f, flPhase = 0, panJ = 0, rtU = 0.5f;
    float pgl = 0, pgr = 0;
    double lastTg = 1e9;
    bool dirty = false;
    Rng rng;
    std::vector<float> ola, wsum, out;
    void alloc()
    {
        ola.assign(kRing, 0.f);
        wsum.assign(kRing, 0.f);
        out.assign(kRing, 0.f);
    }
};

class Engine {
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        shA = 1.f - std::exp(-6.2831853f * gShelfHz / (float)sampleRate);
        hist.assign(kRing, 0.f);
        histL.assign(kRing, 0.f);
        histR.assign(kRing, 0.f);
        histB.assign(kRing, 0.5f);
        eL = eR = 0.f;
        balS = 0.5f;
        for (auto& v : vc) v.alloc();
        reset();
        applyLatency();
    }

    void reset()
    {
        w = 0;
        sinceDetect = 0;
        nDet = 0;
        nMarks = 0;
        haveMark = false;
        started = false;
        std::fill(hist.begin(), hist.end(), 0.f);
        std::fill(histL.begin(), histL.end(), 0.f);
        std::fill(histR.begin(), histR.end(), 0.f);
        std::fill(histB.begin(), histB.end(), 0.5f);
        eL = eR = 0.f;
        balS = 0.5f;
        for (auto& v : vc) {
            v.active = false;
            v.csInit = false;
            v.frontier = INT64_MAX;
            std::fill(v.ola.begin(), v.ola.end(), 0.f);
            std::fill(v.wsum.begin(), v.wsum.end(), 0.f);
            std::fill(v.out.begin(), v.out.end(), 0.f);
        }
        outTime = 0;
    }

    void setParams(const Params& np)
    {
        bool reseed = np.seed != P.seed || np.flangeRate != P.flangeRate;
        bool lat = np.minF0 != P.minF0 || np.latK != P.latK;
        P = np;
        P.voices = std::max(1, std::min(kMaxVoices, P.voices));
        buildScale();
        if (reseed || !seeded) initVoices();
        if (lat) applyLatency();
        seeded = true;
    }

    int latency() const { return L; }
    float currentF0() const { return lastF0; }
    float guiIn() const { return guiInHz.load(std::memory_order_relaxed); }
    float guiOut() const { return guiOutHz.load(std::memory_order_relaxed); }
    int64_t lastCenter() const { return nDet ? dets[(nDet - 1) % kDetRing].center : 0; }
    int markCount() const { return nMarks; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        int64_t w0 = w;
        for (int i = 0; i < n; i++) {
            float xl = inL[i], xr = inR ? inR[i] : inL[i];
            hist[w & kMask] = 0.5f * (xl + xr);
            histL[w & kMask] = xl;
            histR[w & kMask] = xr;
            {
                const float k = 1.f - std::exp(-1.f / (0.02f * (float)sr));
                eL += k * (std::fabs(xl) - eL);
                eR += k * (std::fabs(xr) - eR);
                float tot = eL + eR;
                if (tot > 1e-4f) balS += 0.2f * ((eR / tot) - balS);
                histB[w & kMask] = balS;
            }
            w++;
            if (++sinceDetect >= kHop) {
                sinceDetect = 0;
                detect();
                updateMarks();
            }
        }
        updateMarks();
        if (!started) {
            outTime = w0 - L;
            started = true;
        }
        {
            float tgt = std::min(1.f, std::max(0.f, P.swarm));
            float a = 1.f - (float)std::exp(-(double)n / (sr * 0.08));
            swS += (tgt - swS) * a;
        }
        const float ve = 1.f + (float)(P.voices - 1) * swS;
        const float widen = std::min(1.f, std::max(0.f, ve - 1.f));
        float tgl[kMaxVoices], tgr[kMaxVoices];
        float ss = 0.f;
        for (int vi = 0; vi < kMaxVoices; vi++) {
            float g = std::min(1.f, std::max(0.f, ve - (float)vi));
            ss += g * g;
            Voice& v = vc[vi];
            float pan = 0.5f;
            if (P.voices > 1) {
                if (P.stereoMode == 1) {
                    pan = (vi % 3 == 0) ? 0.5f : (vi % 3 == 1 ? 0.f : 1.f);
                    pan = std::min(1.f, std::max(0.f, pan + v.panJ * 0.04f));
                } else {
                    pan = (vi % 2 == 0) ? 0.f : 1.f;
                    float j = v.panJ * 0.12f;
                    pan = std::min(1.f, std::max(0.f, pan + (pan == 0.f ? j : -j)));
                }
            }
            pan = 0.5f + (pan - 0.5f) * widen;
            if (P.solo >= 0 && vi != P.solo) g = 0.f;
            tgl[vi] = std::cos(pan * 1.5707963f) * 1.4142136f * g;
            tgr[vi] = std::sin(pan * 1.5707963f) * 1.4142136f * g;
        }
        const float norm = 1.0f / std::sqrt(std::max(1.f, ss));
        for (int vi = 0; vi < kMaxVoices; vi++) {
            tgl[vi] *= norm;
            tgr[vi] *= norm;
            if (tgl[vi] > 1e-4f || tgr[vi] > 1e-4f || vc[vi].pgl > 1e-4f || vc[vi].pgr > 1e-4f) { synthVoice(vc[vi], vi); vc[vi].dirty = true; }
            else {
                Voice& q = vc[vi];
                q.active = false;
                q.csInit = false;
                q.frontier = INT64_MAX;
                if (q.dirty) {
                    std::fill(q.ola.begin(), q.ola.end(), 0.f);
                    std::fill(q.wsum.begin(), q.wsum.end(), 0.f);
                    std::fill(q.out.begin(), q.out.end(), 0.f);
                    q.dirty = false;
                }
            }
        }
        const double twoPi = 6.283185307179586;
        const float invn = 1.0f / (float)n;
        for (int i = 0; i < n; i++) {
            int64_t o = outTime + i;
            float dryL = histL[o & kMask], dryR = histR[o & kMask];
            float bal = histB[o & kMask];
            float balL = std::cos(bal * 1.5707963f) * 1.4142136f;
            float balR = std::sin(bal * 1.5707963f) * 1.4142136f;
            float sl = 0, sr_ = 0;
            float fi = (float)(i + 1) * invn;
            for (int vi = 0; vi < kMaxVoices; vi++) {
                Voice& v = vc[vi];
                float gl = v.pgl + (tgl[vi] - v.pgl) * fi;
                float gr = v.pgr + (tgr[vi] - v.pgr) * fi;
                if (!v.active && gl < 1e-5f && gr < 1e-5f && v.pgl < 1e-5f && v.pgr < 1e-5f) continue;
                finalizeSample(v, o);
                float flBase = vi == 0 ? 0.f : P.flangeMs * v.flU1 + P.delaySpreadMs * v.flU2;
                float d = flBase + P.flangeDepthMs * swS * 0.5f * (1.0f + (float)std::sin(twoPi * v.flRate * (double)o / sr + v.flPhase));
                float ds = d * 0.001f * (float)sr;
                int di = (int)ds;
                float fr = ds - di;
                int64_t p0 = o - di;
                float a = v.out[p0 & kMask];
                float b = v.out[(p0 - 1) & kMask];
                float y = a + (b - a) * fr;
                sl += y * gl;
                sr_ += y * gr;
            }
            float wl = sl * balL, wr = sr_ * balR;
            {
                float mk = 1.f + gMake * widen;
                shL += shA * (wl - shL);
                shR += shA * (wr - shR);
                float ks = gShelf * widen;
                wl = (wl + ks * (wl - shL)) * mk;
                wr = (wr + ks * (wr - shR)) * mk;
            }
            outL[i] = (dryL * (1.0f - P.wet) + wl * P.wet) * P.outGain;
            outR[i] = (dryR * (1.0f - P.wet) + wr * P.wet) * P.outGain;
        }
        for (int vi = 0; vi < kMaxVoices; vi++) {
            vc[vi].pgl = tgl[vi];
            vc[vi].pgr = tgr[vi];
        }
        outTime += n;
    }

private:
    struct Det {
        int64_t center;
        float f0;
    };

    double sr = 44100;
    Params P;
    bool seeded = false;
    int L = 512;
    std::vector<float> hist, histL, histR, histB;
    float eL = 0.f, eR = 0.f, balS = 0.5f;
    float shL = 0.f, shR = 0.f, shA = 0.f;
    int64_t w = 0;
    int sinceDetect = 0;
    int64_t outTime = 0;
    bool started = false;
    Det dets[kDetRing];
    int nDet = 0;
    float rawHist[3] = {0, 0, 0};
    float lastF0 = 0;
    std::atomic<float> guiInHz{0.f};
    std::atomic<float> guiOutHz{0.f};
    int64_t marks[kMarkRing];
    int nMarks = 0;
    bool haveMark = false;
    Voice vc[kMaxVoices];
    bool allowed[12];
    float swS = 1.f;
    int holdN = 0;

    void applyLatency() { L = (int)std::ceil((double)P.latK * sr / std::max(40.f, P.minF0)) + 64; }

    void buildScale()
    {
        static const int minor[7] = {0, 2, 3, 5, 7, 8, 10};
        static const int major[7] = {0, 2, 4, 5, 7, 9, 11};
        for (int i = 0; i < 12; i++) allowed[i] = (P.scaleType == 2);
        if (P.scaleType == 2) return;
        const int* s = P.scaleType == 0 ? minor : major;
        for (int k = 0; k < 7; k++) allowed[(P.root + s[k]) % 12] = true;
    }

    void initVoices()
    {
        for (int vi = 0; vi < kMaxVoices; vi++) {
            Voice& v = vc[vi];
            v.rng = Rng(P.seed * 7919u + vi * 104729u + 1u);
            Rng& r = v.rng;
            v.det = r.range(-1.f, 1.f);
            v.driftScale = r.range(0.6f, 1.2f);
            v.driftHzScale = r.range(0.6f, 1.6f);
            v.flU1 = r.range(0.5f, 1.5f);
            v.flU2 = r.range(0.f, 1.f);
            v.flRate = P.flangeRate > 0 ? P.flangeRate * r.range(0.85f, 1.15f) : r.range(0.2f, 1.1f);
            v.flPhase = r.range(0.f, 6.2831853f);
            v.panJ = r.range(-1.f, 1.f);
            v.rtU = r.range(0.f, 1.f);
            v.lastTg = 1e9;
            v.pgl = 0.f;
            v.pgr = 0.f;
            v.active = false;
            v.csInit = false;
        }
    }

    float f0At(int64_t t) const
    {
        if (nDet == 0) return 0;
        int n = std::min(nDet, kDetRing);
        for (int k = 0; k < n; k++) {
            const Det& d = dets[(nDet - 1 - k) % kDetRing];
            if (d.center <= t) return d.f0;
        }
        return dets[(nDet - n) % kDetRing].f0;
    }

    void detect()
    {
        const double sd = sr * 0.5;
        const int tauMax = (int)(sd / 70.0);
        int J = std::max(160, std::min(520, L - 16 - tauMax));
        const int tauMin = std::max(2, (int)(sd / 900.0));
        const int W = J + tauMax;
        if (w < 2 * W + 4) return;
        static thread_local std::vector<float> dec, d, cm;
        dec.resize(W);
        d.assign(tauMax + 1, 0.f);
        cm.assign(tauMax + 1, 0.f);
        int64_t base = w - 2 * W;
        for (int i = 0; i < W; i++) dec[i] = 0.5f * (hist[(base + 2 * i) & kMask] + hist[(base + 2 * i + 1) & kMask]);
        double e = 0;
        for (int i = 0; i < 512; i++) {
            float x = hist[(w - 1 - i) & kMask];
            e += x * x;
        }
        float rms = (float)std::sqrt(e / 512.0);
        float f0 = 0;
        if (rms > gGate) {
            for (int tau = 1; tau <= tauMax; tau++) {
                double s = 0;
                for (int j = 0; j < J; j++) {
                    float df = dec[j] - dec[j + tau];
                    s += df * df;
                }
                d[tau] = (float)s;
            }
            double run = 0;
            cm[0] = 1.f;
            for (int tau = 1; tau <= tauMax; tau++) {
                run += d[tau];
                cm[tau] = run > 0 ? (float)(d[tau] * tau / run) : 1.f;
            }
            int best = -1;
            for (int tau = tauMin; tau < tauMax; tau++) {
                if (cm[tau] < gThr) {
                    while (tau + 1 < tauMax && cm[tau + 1] < cm[tau]) tau++;
                    best = tau;
                    break;
                }
            }
            if (best < 0) {
                float mn = 1e9f;
                for (int tau = tauMin; tau < tauMax; tau++)
                    if (cm[tau] < mn) { mn = cm[tau]; best = tau; }
                if (mn > gFall) best = -1;
            }
            if (best < 0 && rawHist[1] > 0) {
                double tp = sd / rawHist[1];
                int lo = std::max(tauMin, (int)(tp * 0.92));
                int hi = std::min(tauMax - 2, (int)(tp * 1.08) + 1);
                int bb = -1;
                float bm = 1e9f;
                for (int tau = lo; tau <= hi; tau++)
                    if (cm[tau] < bm) { bm = cm[tau]; bb = tau; }
                if (bb > 0 && bm < gCont) best = bb;
            }
            if (best > 1 && best < tauMax) {
                float a = cm[best - 1], b = cm[best], c = cm[best + 1];
                float den = a - 2 * b + c;
                float off = den != 0 ? 0.5f * (a - c) / den : 0.f;
                float tauf = best + std::max(-1.f, std::min(1.f, off));
                f0 = (float)(sd / tauf);
                int t0 = (int)std::lround(2.0 * tauf);
                int N = std::min(1800, 2 * W - t0 - 8);
                if (N > 128) {
                    int64_t s0 = w - 2 * W;
                    double rr[7];
                    for (int k = -3; k <= 3; k++) {
                        int tt = t0 + k;
                        double nu = 0, ea = 0, eb = 0;
                        for (int j = 0; j < N; j++) {
                            float a = hist[(s0 + j) & kMask], b = hist[(s0 + j + tt) & kMask];
                            nu += a * b;
                            ea += a * a;
                            eb += b * b;
                        }
                        rr[k + 3] = nu / std::sqrt(ea * eb + 1e-12);
                    }
                    int bk = 0;
                    for (int k = 1; k < 7; k++) if (rr[k] > rr[bk]) bk = k;
                    if (bk > 0 && bk < 6 && rr[bk] > 0.5) {
                        double dn = rr[bk - 1] - 2 * rr[bk] + rr[bk + 1];
                        double fo = dn != 0 ? 0.5 * (rr[bk - 1] - rr[bk + 1]) / dn : 0.0;
                        double tau2 = t0 + (bk - 3) + std::max(-0.5, std::min(0.5, fo));
                        if (std::fabs(tau2 - 2.0 * tauf) < 2.5) f0 = (float)(sr / tau2);
                    }
                }
            }
        }
        if (f0 <= 0 && rawHist[1] > 0 && holdN < gHold) {
            f0 = rawHist[1];
            holdN++;
        } else if (f0 > 0) {
            holdN = 0;
        }
        float m = f0;
        if (f0 > 0 && rawHist[0] > 0 && rawHist[1] > 0) {
            float a = rawHist[0], b = rawHist[1], c = f0;
            m = std::max(std::min(a, b), std::min(std::max(a, b), c));
        }
        rawHist[0] = rawHist[1];
        rawHist[1] = f0;
        Det dt;
        dt.center = w - W;
        dt.f0 = m;
        dets[nDet % kDetRing] = dt;
        nDet++;
        lastF0 = m;
    }

    void updateMarks()
    {
        if (nDet == 0) return;
        if (!haveMark) {
            const Det& nd = dets[(nDet - 1) % kDetRing];
            if (nd.f0 <= 0) return;
            int T = (int)(sr / nd.f0);
            int64_t lo = nd.center;
            int64_t hi = lo + T;
            if (hi >= w) return;
            int64_t bj = lo;
            float bv = -1;
            for (int64_t j = lo; j < hi; j++) {
                float v = std::fabs(hist[j & kMask]);
                if (v > bv) { bv = v; bj = j; }
            }
            nMarks = 0;
            marks[nMarks++ % kMarkRing] = bj;
            haveMark = true;
        }
        for (;;) {
            int64_t last = marks[(nMarks - 1) % kMarkRing];
            float f = f0At(last);
            if (f <= 0) {
                const Det& nd = dets[(nDet - 1) % kDetRing];
                if (nd.f0 <= 0 && nd.center < last) { haveMark = false; return; }
                f = nd.f0 > 0 ? nd.f0 : 150.f;
            }
            float T = (float)(sr / f);
            int64_t expected = last + (int64_t)T;
            int64_t lo = expected - (int64_t)(0.25f * T);
            int64_t hi = expected + (int64_t)(0.25f * T);
            if (hi >= w) return;
            int64_t bj = expected;
            float bv = -1;
            for (int64_t j = lo; j <= hi; j++) {
                float v = std::fabs(hist[j & kMask]);
                if (v > bv) { bv = v; bj = j; }
            }
            if (bj <= last + (int64_t)(0.5f * T)) bj = expected;
            marks[nMarks++ % kMarkRing] = bj;
        }
    }

    double snapMidi(double m) const
    {
        int k0 = (int)std::floor(m);
        double best = m;
        double bd = 1e9;
        for (int k = k0 - 2; k <= k0 + 3; k++) {
            int pc = ((k % 12) + 12) % 12;
            if (!allowed[pc]) continue;
            double dd = std::fabs(k - m);
            if (dd < bd) { bd = dd; best = k; }
        }
        return best;
    }

    double snapHyst(double m, double last) const
    {
        double best = snapMidi(m);
        if (last < 1e8 && last != best) {
            int pc = (((int)std::llround(last) % 12) + 12) % 12;
            if (allowed[pc] && std::fabs(last - m) <= std::fabs(best - m) + (double)gHyst) return last;
        }
        return best;
    }

    void synthVoice(Voice& v, int vi)
    {
        const double offCents = 1200.0 * std::log2(P.refHz / 440.0);
        for (;;) {
            if (!v.active) {
                if (nMarks == 0) break;
                int64_t m = marks[(nMarks - 1) % kMarkRing];
                if (f0At(m) <= 0) break;
                v.ts = (double)m;
                v.active = true;
                v.csInit = false;
                v.lastTg = 1e9;
            }
            float f = f0At((int64_t)v.ts);
            if (f <= 0) {
                v.active = false;
                v.frontier = INT64_MAX;
                if (&v == &vc[0]) {
                    guiInHz.store(0.f, std::memory_order_relaxed);
                    guiOutHz.store(0.f, std::memory_order_relaxed);
                }
                break;
            }
            double T = sr / f;
            int nsub = 1;
            while (nsub < 6 && (double)P.latK * (T / nsub) + 24.0 > (double)L) nsub++;
            double Tg = T / nsub;
            int half = (int)Tg;
            int64_t mf = -1;
            int n = std::min(nMarks, kMarkRing);
            int idx = -1;
            for (int k = 0; k < n; k++) {
                int64_t mk = marks[(nMarks - 1 - k) % kMarkRing];
                if (mk + half + 2 <= w) { mf = mk; idx = k; break; }
            }
            if (mf < 0) break;
            int64_t ca;
            int64_t mr = mf;
            if (v.ts <= (double)mf) {
                double bd = std::fabs((double)mf - v.ts);
                for (int k = idx + 1; k < n; k++) {
                    int64_t mk = marks[(nMarks - 1 - k) % kMarkRing];
                    double dd = std::fabs((double)mk - v.ts);
                    if (dd <= bd) { bd = dd; mr = mk; } else break;
                }
            }
            {
                int64_t kk = (int64_t)std::llround((v.ts - (double)mr) / Tg);
                if (v.ts > (double)mf && kk < 1) kk = 1;
                ca = mr + (int64_t)std::llround(kk * Tg);
                if (ca + half + 2 > w) break;
            }
            double midi = 69.0 + 12.0 * std::log2((double)f / 440.0) - offCents / 100.0;
            float dr = 0.f;
            if (P.driftCents > 0.f && swS > 0.f) {
                double dtv = Tg / sr;
                double a = std::exp(-6.2831853 * P.driftHz * v.driftHzScale * dtv);
                v.driftState = (float)(a * v.driftState + (1.0 - a) * v.rng.gauss());
                float nrm = (float)std::sqrt((1.0 + a) / std::max(1e-6, 1.0 - a));
                dr = v.driftState * nrm * P.driftCents * swS * v.driftScale;
            }
            double dt0 = (double)v.det * P.spreadCents * swS;
            double c;
            if (!P.tune) {
                c = dt0 + dr;
            } else if (P.preSnap) {
                double tg = snapHyst(midi + (dt0 + dr) / 100.0, v.lastTg);
                v.lastTg = tg;
                c = (tg - midi) * 100.0;
            } else {
                double tg = snapHyst(midi, v.lastTg);
                v.lastTg = tg;
                c = (tg - midi) * 100.0 + dt0 + dr;
            }
            if (!v.csInit) {
                v.cs = c;
                v.csInit = true;
            } else {
                double rt = (double)P.retuneMs * (0.6 + 1.6 * v.rtU) + (vi > 0 ? 12.0 * v.rtU * swS : 0.0);
                if (P.retuneMs <= 0.5f && vi == 0) rt = 0.0;
                double a2 = rt <= 0.5 ? 0.0 : std::exp(-(Tg / sr * 1000.0) / rt);
                v.cs = a2 * v.cs + (1.0 - a2) * c;
            }
            double ratio = std::pow(2.0, v.cs / 1200.0);
            double Tt = Tg / ratio;
            if (&v == &vc[0]) {
                guiInHz.store(f, std::memory_order_relaxed);
                guiOutHz.store((float)(f * ratio), std::memory_order_relaxed);
            }
            int Lg = 2 * half;
            int64_t a0 = ca - half;
            int64_t s0 = (int64_t)std::llround(v.ts) - half;
            for (int q = 0; q < Lg; q++) {
                int64_t pos = s0 + q;
                if (pos < v.emitted) continue;
                float wt = 0.5f - 0.5f * std::cos(6.2831853f * (q + 1) / (Lg + 1));
                if (s0 < v.emitted) wt *= std::min(1.f, (float)(pos - v.emitted + 1) * (1.f / 96.f));
                v.ola[pos & kMask] += hist[(a0 + q) & kMask] * wt;
                v.wsum[pos & kMask] += wt;
            }
            v.ts += Tt;
            v.frontier = (int64_t)(v.ts - Tg) - 2;
        }
    }

    void finalizeSample(Voice& v, int64_t o)
    {
        int ix = (int)(o & kMask);
        float ol = v.ola[ix], ws = v.wsum[ix];
        v.ola[ix] = 0.f;
        v.wsum[ix] = 0.f;
        float dry = hist[ix];
        float t = std::min(1.f, std::max(0.f, (ws - 0.15f) / 0.45f));
        float g = t * t * (3.f - 2.f * t);
        if (gTrace && g < 0.999f) gTrace->push_back({o, ws});
        float y = g * (ol / std::max(ws, 1e-3f)) + (1.f - g) * dry;
        v.out[ix] = y;
        v.emitted = o + 1;
    }
};

}
