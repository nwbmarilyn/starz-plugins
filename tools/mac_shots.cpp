#include "gui.h"
#include "fxgui.h"
#include <ImageIO/ImageIO.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static int gFail = 0;

static void check(bool ok, const std::string& what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) gFail++;
}

static void savePng(CGContextRef ctx, const std::string& path)
{
    CGImageRef im = CGBitmapContextCreateImage(ctx);
    CFStringRef p = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, p, kCFURLPOSIXPathStyle, false);
    CGImageDestinationRef d = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
    CGImageDestinationAddImage(d, im, nullptr);
    CGImageDestinationFinalize(d);
    CFRelease(d);
    CFRelease(url);
    CFRelease(p);
    CGImageRelease(im);
}

template <class V>
static void shot(V* v, int w, int h, float scale, const std::string& path)
{
    Gdiplus::gDeviceScale = scale;
    int pw = (int)(w * scale), ph = (int)(h * scale);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(nullptr, pw, ph, 8, 0, cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, pw, ph));
    CGContextTranslateCTM(ctx, 0, ph);
    CGContextScaleCTM(ctx, scale, -scale);
    {
        Gdiplus::Graphics g(ctx);
        v->paintAll(g);
    }
    savePng(ctx, path);
    CGContextRelease(ctx);
}

static double vN[34];
static const double vmn[34] = {0, 0, 0, 0, 415, 0, 0, 1, 0, 0, 0.1, 0, 0, 0, 0, 0, 60, -12, 1, 0, 0, 0, 0.2, 0, 1500, 0, 0, 0, 40, 0, 40, 1500, 0, 0};
static const double vmx[34] = {2, 100, 11, 2, 466, 300, 1, 12, 100, 150, 6, 15, 4, 30, 1, 1, 250, 12, 16, 100, 2, 100, 8, 120, 16000, 100, 100, 100, 1200, 95, 800, 16000, 100, 100};
static const double vdf[34] = {2, 100, 7, 0, 440, 4, 1, 8, 25, 40, 1.2, 7, 1, 12, 0, 1, 100, 0, 1, 100, 1, 0, 1.8, 20, 7000, 100, 50, 0, 300, 40, 250, 5500, 100, 50};
static const int vst[34] = {2, 0, 11, 2, 0, 0, 1, 11, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 15, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const wchar_t* vnames[3] = {L"Auto-Tune", L"Auto-Tune Hard (Artist)", L"Mr Kitty"};
static double tsec = 0;

struct VBridge : gui::Bridge {
    float getNorm(int id) override { return (float)vN[id]; }
    void edit(int id, double n, int phase) override { if (phase == 1) vN[id] = n; }
    std::wstring text(int id) override
    {
        double p = vmn[id] + vN[id] * (vmx[id] - vmn[id]);
        wchar_t b[64];
        if (id == 0) return vnames[(int)std::lround(p)];
        if (id == 2) { static const wchar_t* k[12] = {L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B"}; return k[(int)std::lround(p)]; }
        if (id == 3) { static const wchar_t* s[3] = {L"Minor", L"Major", L"Chromatic"}; return s[(int)std::lround(p)]; }
        if (id == 20) { static const wchar_t* lm[3] = {L"Clean", L"Balanced", L"Live"}; return lm[(int)std::lround(p)]; }
        if (id == 14) return p > 0.5 ? L"L-C-R" : L"Wide";
        if (id == 6 || id == 15) return p > 0.5 ? L"On" : L"Off";
        swprintf(b, 64, L"%.*f", (id == 7 || id == 18) ? 0 : 1, p);
        return b;
    }
    float pitchIn() override
    {
        double n = 55 + 3 * std::sin(tsec * 0.9) + (std::fmod(tsec, 2.0) < 1 ? 0 : 2.4) + 0.3 * std::sin(tsec * 17);
        return (float)(436.0 * std::pow(2.0, (n - 69) / 12.0));
    }
    float pitchOut() override
    {
        double n = 55 + 3 * std::sin(tsec * 0.9) + (std::fmod(tsec, 2.0) < 1 ? 0 : 2.4);
        n = std::round(n);
        return (float)(436.0 * std::pow(2.0, (n - 69) / 12.0));
    }
    float refHz() override { return 436; }
    float latencyMs() override { return 15.7f; }
    int keyRoot() override { return 7; }
    int scaleType() override { return 0; }
};

static double fN[16];
static int kind = 0;
static const double rmn[10] = {0, 0, 0, 0.2, 0, 1500, 20, 0, 0, 0};
static const double rmx[10] = {7, 100, 100, 10, 150, 16000, 800, 100, 100, 100};
static const double rdf[10] = {0, 35, 100, 0.9, 15, 12000, 200, 100, 30, 40};
static const int rst[10] = {7, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const double dmn[12] = {0, 0, 0, 0, 20, 0, 20, 1500, 0, 0, 0, 0};
static const double dmx[12] = {7, 100, 100, 5, 1500, 95, 1000, 18000, 100, 100, 100, 100};
static const double ddf[12] = {0, 35, 100, 2, 300, 40, 250, 5500, 100, 50, 0, 20};
static const int dst[12] = {7, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0};
static const wchar_t* rnames[8] = {L"Klyukva Air", L"Vocal Plate", L"Tight Room", L"Dark Hall", L"Huge Bright", L"Ghost", L"Cathedral Dream", L"Send Only"};
static const wchar_t* dnames[8] = {L"Eighth Ping-Pong", L"Slapback", L"Dark Tape", L"Quarter Echo", L"Long Wash", L"Triplet Bounce", L"Wide Doubler", L"Send Only"};
static const double* fmn() { return kind ? dmn : rmn; }
static const double* fmx() { return kind ? dmx : rmx; }

struct FBridge : fxgui::Bridge {
    float getNorm(int id) override { return (float)fN[id]; }
    double getPlain(int id) override { return fmn()[id] + fN[id] * (fmx()[id] - fmn()[id]); }
    void edit(int id, double n, int phase) override { if (phase == 1) fN[id] = n; }
    std::wstring text(int id) override
    {
        double p = getPlain(id);
        if (id == 0) return kind ? dnames[(int)std::lround(p)] : rnames[(int)std::lround(p)];
        if (kind && id == 3) { static const wchar_t* s[6] = {L"Off", L"1/4", L"1/8", L"1/8 dotted", L"1/16", L"1/8 triplet"}; return s[(int)std::lround(p)]; }
        wchar_t b[64];
        swprintf(b, 64, L"%.*f", (!kind && id == 3) ? 1 : 0, p);
        return b;
    }
    float effectiveDelayMs() override { return (float)getPlain(4) > 0 && getPlain(3) < 0.5 ? (float)getPlain(4) : 250.f; }
};

template <class V>
static void behave(V* v, double* n, const double* mn, const double* mx, const double* df, int knobId, int kx, int ky, const std::string& t)
{
    double before = n[knobId];
    v->mouseDown(kx, ky, false);
    v->mouseMove(kx, ky - 40, false);
    v->mouseUp();
    check(n[knobId] > before + 0.2, t + " drag up raises knob");
    double mid = n[knobId];
    v->mouseDown(kx, ky, false);
    v->mouseMove(kx, ky - 40, true);
    v->mouseUp();
    check(n[knobId] > mid && n[knobId] - mid < 0.05, t + " shift drag is fine");
    v->rightDown(kx, ky);
    check(std::fabs(n[knobId] - (df[knobId] - mn[knobId]) / (mx[knobId] - mn[knobId])) < 0.01, t + " right click resets");
    double w0 = n[knobId];
    v->wheel(kx, ky, 1);
    check(n[knobId] > w0, t + " wheel up raises");
    double p0 = n[0];
    v->mouseDown(363, 30, false);
    check(n[0] != p0, t + " left arrow changes preset");
    double p1 = n[0];
    v->mouseDown(585, 30, false);
    check(n[0] != p1, t + " right arrow changes preset");
}

int main(int argc, char** argv)
{
    std::string dir = argc > 1 ? argv[1] : ".";
    ui::acquire();
    for (int i = 0; i < 34; i++) vN[i] = (vdf[i] - vmn[i]) / (vmx[i] - vmn[i]);
    vN[0] = 1.0;
    VBridge vb;
    gui::View* vv = new gui::View(&vb, 34, vmn, vmx, vdf, vst, vnames);
    vv->init();
    for (int i = 0; i < 120; i++) {
        tsec = i * 0.033;
        vv->tick();
    }
    shot(vv, 760, 524, 2.f, dir + "/mac-vocal.png");
    shot(vv, 760, 524, 1.f, dir + "/mac-vocal-1x.png");
    behave(vv, vN, vmn, vmx, vdf, 5, 70, 352, "vocal");
    vv->mouseMove(70, 352, false);
    shot(vv, 760, 524, 2.f, dir + "/mac-vocal-hover.png");

    for (int k = 0; k < 2; k++) {
        kind = k;
        int np = k ? 12 : 10;
        for (int i = 0; i < np; i++) fN[i] = ((k ? ddf : rdf)[i] - fmn()[i]) / (fmx()[i] - fmn()[i]);
        FBridge fb;
        fxgui::Layout lay = k ? fxgui::Layout{false, L"STARZ", L"delay", fxgui::cPink, fxgui::cAqua} : fxgui::Layout{true, L"STARZ", L"reverb", fxgui::cPink, fxgui::cAqua};
        fxgui::View* fv = new fxgui::View(&fb, lay, np, fmn(), fmx(), k ? ddf : rdf, k ? dst : rst, k ? dnames : rnames, 8);
        fv->init();
        for (int i = 0; i < 40; i++) fv->tick();
        std::string nm = k ? "delay" : "reverb";
        shot(fv, 760, 480, 2.f, dir + "/mac-" + nm + ".png");
        behave(fv, fN, fmn(), fmx(), k ? ddf : rdf, k ? 4 : 3, 316, 306, nm);
    }
    std::printf("failures %d\n", gFail);
    return gFail ? 1 : 0;
}
