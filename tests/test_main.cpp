#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "engine.h"

static bool readWav(const char* path, std::vector<float>& mono, int& sr)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char id[4];
    uint32_t sz;
    fread(id, 1, 4, f);
    fread(&sz, 4, 1, f);
    fread(id, 1, 4, f);
    int ch = 1, bits = 16, fmt = 1;
    sr = 44100;
    while (fread(id, 1, 4, f) == 4 && fread(&sz, 4, 1, f) == 1) {
        if (!memcmp(id, "fmt ", 4)) {
            uint16_t a, c, b;
            uint32_t r, br;
            uint16_t ba;
            fread(&a, 2, 1, f);
            fread(&c, 2, 1, f);
            fread(&r, 4, 1, f);
            fread(&br, 4, 1, f);
            fread(&ba, 2, 1, f);
            fread(&b, 2, 1, f);
            fmt = a;
            ch = c;
            sr = (int)r;
            bits = b;
            if (sz > 16) fseek(f, sz - 16, SEEK_CUR);
        } else if (!memcmp(id, "data", 4)) {
            int bytes = bits / 8;
            size_t n = sz / (bytes * ch);
            mono.resize(n);
            std::vector<uint8_t> buf(sz);
            fread(buf.data(), 1, sz, f);
            for (size_t i = 0; i < n; i++) {
                double s = 0;
                for (int c = 0; c < ch; c++) {
                    const uint8_t* p = &buf[(i * ch + c) * bytes];
                    double v;
                    if (fmt == 3 && bits == 32) { float x; memcpy(&x, p, 4); v = x; }
                    else if (bits == 16) { int16_t x; memcpy(&x, p, 2); v = x / 32768.0; }
                    else if (bits == 24) { int32_t x = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = x / 2147483648.0; }
                    else { int32_t x; memcpy(&x, p, 4); v = x / 2147483648.0; }
                    s += v;
                }
                mono[i] = (float)(s / ch);
            }
            fclose(f);
            return true;
        } else {
            fseek(f, sz, SEEK_CUR);
        }
    }
    fclose(f);
    return false;
}

static void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = fopen(path, "wb");
    uint32_t n = (uint32_t)l.size();
    uint32_t dsz = n * 2 * 4, rsz = 36 + dsz, fs = 16, br = sr * 8;
    uint16_t fm = 3, ch = 2, ba = 8, bt = 32;
    fwrite("RIFF", 1, 4, f);
    fwrite(&rsz, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fs, 4, 1, f);
    fwrite(&fm, 2, 1, f);
    fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f);
    fwrite(&bt, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dsz, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) {
        fwrite(&l[i], 4, 1, f);
        fwrite(&r[i], 4, 1, f);
    }
    fclose(f);
}

int main(int argc, char** argv)
{
    if (argc < 4) return 1;
    std::vector<float> in;
    int sr;
    if (!readWav(argv[1], in, sr)) { printf("read fail\n"); return 2; }
    std::string preset = argv[3];
    int block = argc > 4 ? atoi(argv[4]) : 128;
    float sweepSec = 0.f;
    float panIn = 0.f;
    ark::Params p = preset == "artist" ? ark::presetArtist() : ark::presetKitty();
    if (preset == "dry") { p = ark::presetArtist(); p.tune = false; }
    for (int i = 5; i + 1 < argc; i += 2) {
        std::string k = argv[i];
        float v = (float)atof(argv[i + 1]);
        if (k == "voices") p.voices = (int)v;
        else if (k == "spread") p.spreadCents = v;
        else if (k == "drift") p.driftCents = v;
        else if (k == "retune") p.retuneMs = v;
        else if (k == "minf0") p.minF0 = v;
        else if (k == "wet") p.wet = v;
        else if (k == "stereo") p.stereoMode = (int)v;
        else if (k == "presnap") p.preSnap = v != 0;
        else if (k == "ref") p.refHz = v;
        else if (k == "seed") p.seed = (uint32_t)v;
        else if (k == "swarm") p.swarm = v;
        else if (k == "sweep") sweepSec = v;
        else if (k == "solo") p.solo = (int)v;
        else if (k == "hyst") ark::gHyst = v;
        else if (k == "latk") p.latK = v;
        else if (k == "pan") panIn = v;
        else if (k == "make") ark::gMake = v;
        else if (k == "shelf") ark::gShelf = v;
        else if (k == "cont") ark::gCont = v;
        else if (k == "hold") ark::gHold = (int)v;
        else if (k == "gate") ark::gGate = v;
        else if (k == "thr") ark::gThr = v;
        else if (k == "fall") ark::gFall = v;
    }
    std::vector<std::pair<int64_t, float>> trace;
    if (getenv("ARK_TRACE")) ark::gTrace = &trace;
    ark::Engine* e = new ark::Engine();
    ark::Params base = p;
    e->prepare(sr);
    e->setParams(p);
    int L = e->latency();
    size_t n = in.size();
    std::vector<float> inp(in);
    inp.resize(n + L + block, 0.f);
    std::vector<float> ol(inp.size()), orr(inp.size());
    std::vector<float> inpL(inp.size()), inpR(inp.size());
    for (size_t q = 0; q < inp.size(); q++) {
        float gl = panIn <= 0.f ? 1.f : 1.f - panIn, gr = panIn >= 0.f ? 1.f : 1.f + panIn;
        inpL[q] = inp[q] * gl;
        inpR[q] = inp[q] * gr;
    }
    auto t0 = std::chrono::steady_clock::now();
    FILE* fc = fopen((std::string(argv[2]) + ".f0.csv").c_str(), "w");
    int64_t lastC = -1;
    for (size_t i = 0; i + block <= inp.size(); i += block) {
        if (sweepSec > 0.f) {
            double ph = std::fmod(i / (double)sr, sweepSec) / sweepSec;
            p.swarm = (float)(ph < 0.5 ? ph * 2.0 : 2.0 - ph * 2.0);
            e->setParams(p);
        }
        e->process(&inpL[i], &inpR[i], &ol[i], &orr[i], block);
        int64_t c = e->lastCenter();
        if (c != lastC) {
            fprintf(fc, "%.5f,%.2f\n", c / (double)sr, e->currentF0());
            lastC = c;
        }
    }
    fclose(fc);
    if (ark::gTrace) {
        FILE* ft = fopen((std::string(argv[2]) + ".trace.csv").c_str(), "w");
        for (auto& q : trace) fprintf(ft, "%.5f,%.3f\n", (q.first - L) / (double)sr, q.second);
        fclose(ft);
    }
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::vector<float> a(ol.begin() + L, ol.begin() + L + n), b(orr.begin() + L, orr.begin() + L + n);
    writeWav(argv[2], a, b, sr);
    printf("latency %d samples = %.2f ms; processed %.1f s audio in %.2f s (%.1f%% of one core)\n", L, 1000.0 * L / sr, n / (double)sr, secs,
           100.0 * secs / (n / (double)sr));
    return 0;
}
