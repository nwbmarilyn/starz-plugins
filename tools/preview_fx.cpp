#define NOMINMAX
#include <algorithm>
#include "fxgui.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>

static double gNorm[16];
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
static const double* mn() { return kind ? dmn : rmn; }
static const double* mx() { return kind ? dmx : rmx; }

struct FakeBridge : fxgui::Bridge {
    float getNorm(int id) override { return (float)gNorm[id]; }
    double getPlain(int id) override { return mn()[id] + gNorm[id] * (mx()[id] - mn()[id]); }
    void edit(int id, double n, int phase) override { if (phase == 1) gNorm[id] = n; }
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

int main(int argc, char** argv)
{
    kind = argc > 2 ? atoi(argv[2]) : 0;
    int np = kind ? 12 : 10;
    for (int i = 0; i < np; i++) gNorm[i] = ((kind ? ddf : rdf)[i] - mn()[i]) / (mx()[i] - mn()[i]);
    FakeBridge fb;
    fxgui::Layout lay = kind ? fxgui::Layout{false, L"STARZ", L"delay", fxgui::cPink, fxgui::cAqua} : fxgui::Layout{true, L"STARZ", L"reverb", fxgui::cPink, fxgui::cAqua};
    fxgui::View* v = new fxgui::View(&fb, lay, np, mn(), mx(), kind ? ddf : rdf, kind ? dst : rst, kind ? dnames : rnames, 8);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"host";
    RegisterClassW(&wc);
    HWND host = CreateWindowExW(WS_EX_TOOLWINDOW, L"host", L"p", WS_POPUP | WS_VISIBLE, -5000, -5000, 760, 480, nullptr, nullptr, wc.hInstance, nullptr);
    v->attached(host, "HWND");
    DWORD t0 = GetTickCount();
    MSG m;
    while (GetTickCount() - t0 < 1500) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(5);
    }
    HWND child = GetWindow(host, GW_CHILD);
    HDC dc = GetDC(host);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bm = CreateCompatibleBitmap(dc, 760, 480);
    SelectObject(mem, bm);
    PrintWindow(child, mem, 0);
    BITMAPINFOHEADER bi = {sizeof(bi), 760, -480, 1, 32, BI_RGB};
    std::vector<unsigned char> px(760 * 480 * 4);
    GetDIBits(mem, bm, 0, 480, px.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    FILE* f = fopen(argc > 1 ? argv[1] : "preview.raw", "wb");
    fwrite(px.data(), 1, px.size(), f);
    fclose(f);
    v->removed();
    return 0;
}
