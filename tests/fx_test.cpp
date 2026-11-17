#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "fx.h"

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
    std::string kind = argv[1];
    std::vector<float> in;
    int sr = 44100;
    std::string src = argv[2];
    if (src == "impulse") {
        in.assign(sr, 0.f);
        in[100] = 1.f;
    } else if (!readWav(argv[2], in, sr)) {
        return 2;
    }
    in.resize(in.size() + (size_t)(6.0 * sr), 0.f);
    float panIn = 0.f;
    ark::ReverbParams rp;
    ark::DelayParams dp;
    for (int i = 4; i + 1 < argc; i += 2) {
        std::string k = argv[i];
        float v = (float)atof(argv[i + 1]);
        if (k == "mix") { rp.mix = v; dp.mix = v; }
        else if (k == "dry") { rp.dry = v; dp.dry = v; }
        else if (k == "decay") rp.decay = v;
        else if (k == "pre") rp.pre = v;
        else if (k == "damp") rp.damp = v;
        else if (k == "lowcut") rp.lowCut = v;
        else if (k == "width") rp.width = v;
        else if (k == "duck") { rp.duck = v; dp.duck = v; }
        else if (k == "mod") rp.mod = v;
        else if (k == "time") dp.timeMs = v;
        else if (k == "feed") dp.feed = v;
        else if (k == "low") dp.low = v;
        else if (k == "high") dp.high = v;
        else if (k == "ping") dp.ping = v;
        else if (k == "wobble") dp.wobble = v;
        else if (k == "drive") dp.drive = v;
        else if (k == "pan") panIn = v;
    }
    size_t n = in.size();
    std::vector<float> ol(n), orr(n);
    std::vector<float> inL(n), inR(n);
    for (size_t q = 0; q < n; q++) {
        inL[q] = in[q] * (panIn > 0.f ? 1.f - panIn : 1.f);
        inR[q] = in[q] * (panIn < 0.f ? 1.f + panIn : 1.f);
    }
    const int block = 128;
    if (kind == "reverb") {
        ark::Reverb* e = new ark::Reverb();
        e->P = rp;
        e->prepare(sr);
        for (size_t i = 0; i + block <= n; i += block) e->process(&inL[i], &inR[i], &ol[i], &orr[i], block);
    } else {
        ark::Delay* e = new ark::Delay();
        e->P = dp;
        e->prepare(sr);
        for (size_t i = 0; i + block <= n; i += block) e->process(&inL[i], &inR[i], &ol[i], &orr[i], block);
    }
    writeWav(argv[3], ol, orr, sr);
    printf("ok %zu\n", n);
    return 0;
}
