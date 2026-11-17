#define NOMINMAX
#include <algorithm>
#include "gui.h"
#include <cstdio>
#include <cmath>

static double gNorm[34];
static const double mn[34] = {0, 0, 0, 0, 415, 0, 0, 1, 0, 0, 0.1, 0, 0, 0, 0, 0, 60, -12, 1, 0, 0, 0, 0.2, 0, 1500, 0, 0, 0, 40, 0, 40, 1500, 0, 0};
static const double mx[34] = {2, 100, 11, 2, 466, 300, 1, 12, 100, 150, 6, 15, 4, 30, 1, 1, 250, 12, 16, 100, 2, 100, 8, 120, 16000, 100, 100, 100, 1200, 95, 800, 16000, 100, 100};
static const double df[34] = {2, 100, 7, 0, 440, 4, 1, 8, 25, 40, 1.2, 7, 1, 12, 0, 1, 100, 0, 1, 100, 1, 0, 1.8, 20, 7000, 100, 50, 0, 300, 40, 250, 5500, 100, 50};
static const int st[34] = {2, 0, 11, 2, 0, 0, 1, 11, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 15, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const wchar_t* names[3] = {L"Auto-Tune", L"Auto-Tune Hard (Artist)", L"Mr Kitty"};
static double tsec = 0;

struct FakeBridge : gui::Bridge {
    float getNorm(int id) override { return (float)gNorm[id]; }
    void edit(int id, double n, int phase) override { if (phase == 1) gNorm[id] = n; }
    std::wstring text(int id) override
    {
        double p = mn[id] + gNorm[id] * (mx[id] - mn[id]);
        wchar_t b[64];
        if (id == 0) return names[(int)std::lround(p)];
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
        if (std::fmod(tsec, 5.0) > 4.5) return 0;
        return (float)(436.0 * std::pow(2.0, (n - 69) / 12.0));
    }
    float pitchOut() override
    {
        double n = 55 + 3 * std::sin(tsec * 0.9) + (std::fmod(tsec, 2.0) < 1 ? 0 : 2.4);
        n = std::round(n);
        if (std::fmod(tsec, 5.0) > 4.5) return 0;
        return (float)(436.0 * std::pow(2.0, (n - 69) / 12.0));
    }
    float refHz() override { return 436; }
    float latencyMs() override { return 15.7f; }
    int keyRoot() override { return 7; }
    int scaleType() override { return 0; }
};

int main(int argc, char** argv)
{
    for (int i = 0; i < 34; i++) gNorm[i] = (df[i] - mn[i]) / (mx[i] - mn[i]);
    gNorm[0] = 1.0;
    FakeBridge fb;
    gui::View* v = new gui::View(&fb, 34, mn, mx, df, st, names);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"host";
    RegisterClassW(&wc);
    HWND host = CreateWindowExW(WS_EX_TOOLWINDOW, L"host", L"p", WS_POPUP | WS_VISIBLE, -5000, -5000, 760, 524, nullptr, nullptr, wc.hInstance, nullptr);
    v->attached(host, "HWND");
    DWORD t0 = GetTickCount();
    MSG m;
    while (GetTickCount() - t0 < 2500) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        tsec = (GetTickCount() - t0) / 1000.0;
        Sleep(5);
    }
    HWND child = GetWindow(host, GW_CHILD);
    HDC dc = GetDC(host);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bm = CreateCompatibleBitmap(dc, 760, 524);
    SelectObject(mem, bm);
    PrintWindow(child, mem, 0);
    BITMAPINFOHEADER bi = {sizeof(bi), 760, -524, 1, 32, BI_RGB};
    std::vector<unsigned char> px(760 * 524 * 4);
    GetDIBits(mem, bm, 0, 524, px.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    FILE* f = fopen(argc > 1 ? argv[1] : "preview.raw", "wb");
    fwrite(px.data(), 1, px.size(), f);
    fclose(f);
    v->removed();
    return 0;
}
