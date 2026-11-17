#pragma once
#define NOMINMAX
#include <algorithm>
#include <windows.h>
#include <objidl.h>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <shlwapi.h>
#include <cmath>
#include <string>
#include <vector>
#include "bridge.h"
#include "brand.h"
#include "cat_data.h"
#include "public.sdk/source/common/pluginview.h"

namespace gui {

using namespace Gdiplus;


enum Kind { kKnob, kSelect, kArrowL, kArrowR };

struct Ctl {
    int id;
    Kind kind;
    float x, y, r;
    float w, h;
    Color col;
    std::wstring label;
};

static ULONG_PTR gToken = 0;
static int gRef = 0;
static Image* gCat = nullptr;

static void gdipAcquire()
{
    if (gRef++ == 0) {
        GdiplusStartupInput in;
        GdiplusStartup(&gToken, &in, nullptr);
        IStream* st = SHCreateMemStream(kCatPng, (UINT)kCatPngSize);
        if (st) {
            gCat = Image::FromStream(st);
            st->Release();
        }
    }
}

static void gdipRelease()
{
    if (--gRef == 0) {
        delete gCat;
        gCat = nullptr;
        GdiplusShutdown(gToken);
    }
}

static const Color cBg1(255, 206, 206, 203), cBg2(255, 192, 192, 190), cCard(255, 224, 224, 222), cCardEdge(255, 17, 17, 17);
static const Color cText(255, 17, 17, 17), cDim(255, 112, 112, 112), cPink(255, 255, 85, 0), cAqua(255, 17, 17, 17),
    cYellow(255, 82, 82, 82), cMint(255, 160, 160, 160);

class View : public Steinberg::CPluginView {
public:
    View(Bridge* b, int nParams, const double* mn, const double* mx, const double* def, const int* steps, const wchar_t* const* presetNames)
        : br(b), np(nParams), pmn(mn), pmx(mx), pdef(def), pst(steps), pnames(presetNames)
    {
        rect = Steinberg::ViewRect(0, 0, kW, kH);
        layout();
    }

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) SMTG_OVERRIDE
    {
        return strcmp(type, "HWND") == 0 ? Steinberg::kResultTrue : Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) SMTG_OVERRIDE
    {
        gdipAcquire();
        static bool reg = false;
        HINSTANCE hi = GetModuleHandleW(nullptr);
        if (!reg) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = &View::proc;
            wc.hInstance = hi;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.lpszClassName = L"StarzVocalView";
            RegisterClassW(&wc);
            reg = true;
        }
        hwnd = CreateWindowExW(0, L"StarzVocalView", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, kW, kH, (HWND)parent, nullptr, hi, this);
        SetTimer(hwnd, 1, 33, nullptr);
        return Steinberg::CPluginView::attached(parent, type);
    }

    Steinberg::tresult PLUGIN_API removed() SMTG_OVERRIDE
    {
        if (hwnd) {
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
            hwnd = nullptr;
            gdipRelease();
        }
        return Steinberg::CPluginView::removed();
    }

    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) SMTG_OVERRIDE
    {
        if (r) rect = *r;
        return Steinberg::kResultTrue;
    }
    Steinberg::tresult PLUGIN_API canResize() SMTG_OVERRIDE { return Steinberg::kResultFalse; }

private:
    static constexpr int kW = 760, kH = 520;
    Bridge* br;
    int np;
    const double *pmn, *pmx, *pdef;
    const int* pst;
    const wchar_t* const* pnames;
    HWND hwnd = nullptr;
    std::vector<Ctl> ctl;
    int drag = -1, hover = -1;
    int dragY = 0;
    float dragN = 0;
    struct Pt { float in, out; };
    std::vector<Pt> hist;
    float viewCenter = 0, noteSmooth = 0;
    float lastOutMidi = 0;

    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        View* v;
        if (m == WM_NCCREATE) {
            v = (View*)((CREATESTRUCTW*)l)->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)v);
        } else {
            v = (View*)GetWindowLongPtrW(h, GWLP_USERDATA);
        }
        if (!v) return DefWindowProcW(h, m, w, l);
        return v->msg(h, m, w, l);
    }

    void layout()
    {
        ctl.clear();
        auto knob = [&](int id, float cx, float cy, float r, Color c, const wchar_t* lab) {
            ctl.push_back({id, kKnob, cx, cy, r, 0, 0, c, lab});
        };
        auto sel = [&](int id, float x, float y, float w, Color c, const wchar_t* lab) {
            ctl.push_back({id, kSelect, x, y, 0, w, 26, c, lab});
        };
        ctl.push_back({0, kArrowL, 366, 20, 0, 24, 24, cText, L""});
        ctl.push_back({0, kArrowR, 574, 20, 0, 24, 24, cText, L""});
        sel(2, 44, 276, 68, cAqua, L"KEY");
        sel(3, 118, 276, 108, cAqua, L"SCALE");
        sel(6, 232, 276, 88, cAqua, L"ENABLE");
        knob(5, 70, 352, 26, cAqua, L"RETUNE");
        knob(4, 154, 352, 26, cAqua, L"REFERENCE");
        knob(16, 238, 352, 26, cAqua, L"LOWEST");
        knob(1, 70, 448, 26, cYellow, L"MIX");
        knob(17, 154, 448, 26, cYellow, L"OUTPUT");
        sel(14, 376, 276, 118, cPink, L"STEREO");
        sel(15, 500, 276, 118, cPink, L"STEPPED");
        sel(18, 624, 276, 88, cPink, L"VAR");
        knob(7, 412, 352, 24, cPink, L"VOICES");
        knob(8, 502, 352, 24, cPink, L"SPREAD");
        knob(9, 592, 352, 24, cPink, L"DRIFT");
        knob(10, 682, 352, 24, cPink, L"RATE");
        knob(11, 412, 448, 24, cPink, L"FLANGE");
        knob(12, 502, 448, 24, cPink, L"DEPTH");
        knob(13, 592, 448, 24, cPink, L"DELAY");
        knob(19, 682, 448, 28, cAqua, L"SWARM");
        sel(20, 204, 470, 124, cYellow, L"MODE");
    }

    int hit(int x, int y)
    {
        for (int i = (int)ctl.size() - 1; i >= 0; i--) {
            const Ctl& c = ctl[i];
            if (c.kind == kKnob) {
                float dx = x - c.x, dy = y - c.y;
                if (dx * dx + dy * dy <= (c.r + 6) * (c.r + 6)) return i;
            } else if (x >= c.x && x <= c.x + c.w && y >= c.y && y <= c.y + c.h) {
                return i;
            }
        }
        return -1;
    }

    double quant(int id, double n)
    {
        n = std::min(1.0, std::max(0.0, n));
        if (pst[id] > 0) n = std::round(n * pst[id]) / pst[id];
        return n;
    }

    void cycle(int id, int dir)
    {
        int n = pst[id];
        if (n <= 0) return;
        int idx = (int)std::lround(br->getNorm(id) * n);
        idx = ((idx + dir) % (n + 1) + (n + 1)) % (n + 1);
        br->edit(id, 0, 0);
        br->edit(id, (double)idx / n, 1);
        br->edit(id, 0, 2);
    }

    LRESULT msg(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            tick();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            paint(dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = (short)LOWORD(l), y = (short)HIWORD(l);
            int i = hit(x, y);
            if (i < 0) return 0;
            const Ctl& c = ctl[i];
            if (c.kind == kArrowL || c.kind == kArrowR) {
                cycle(0, c.kind == kArrowL ? -1 : 1);
            } else if (c.kind == kSelect) {
                cycle(c.id, (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1);
            } else {
                drag = i;
                dragY = y;
                dragN = br->getNorm(c.id);
                SetCapture(h);
                br->edit(c.id, dragN, 0);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK:
            return 0;
        case WM_MOUSEMOVE: {
            int x = (short)LOWORD(l), y = (short)HIWORD(l);
            if (drag >= 0) {
                const Ctl& c = ctl[drag];
                double sens = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.0008 : 0.006;
                double n = quant(c.id, dragN + (dragY - y) * sens);
                br->edit(c.id, n, 1);
            } else {
                hover = hit(x, y);
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (drag >= 0) {
                br->edit(ctl[drag].id, br->getNorm(ctl[drag].id), 2);
                drag = -1;
                ReleaseCapture();
            }
            return 0;
        case WM_RBUTTONDOWN: {
            int i = hit((short)LOWORD(l), (short)HIWORD(l));
            if (i >= 0 && ctl[i].kind == kKnob) {
                int id = ctl[i].id;
                br->edit(id, 0, 0);
                br->edit(id, quant(id, (pdef[id] - pmn[id]) / (pmx[id] - pmn[id])), 1);
                br->edit(id, 0, 2);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT p = {(short)LOWORD(l), (short)HIWORD(l)};
            ScreenToClient(h, &p);
            int i = hit(p.x, p.y);
            if (i >= 0 && ctl[i].kind == kKnob) {
                int id = ctl[i].id;
                int d = GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1 : -1;
                double step = pst[id] > 0 ? 1.0 / pst[id] : 0.02;
                br->edit(id, 0, 0);
                br->edit(id, quant(id, br->getNorm(id) + d * step), 1);
                br->edit(id, 0, 2);
            } else if (i >= 0 && ctl[i].kind == kSelect) {
                cycle(ctl[i].id, GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1 : -1);
            }
            return 0;
        }
        }
        return DefWindowProcW(h, m, w, l);
    }

    void tick()
    {
        float fi = br->pitchIn(), fo = br->pitchOut();
        Pt p;
        float ref = br->refHz();
        p.in = fi > 0 ? 69.f + 12.f * std::log2(fi / ref) : -1000.f;
        p.out = fo > 0 ? 69.f + 12.f * std::log2(fo / ref) : -1000.f;
        hist.push_back(p);
        if (hist.size() > 200) hist.erase(hist.begin());
        if (p.out > -500) {
            lastOutMidi = p.out;
            noteSmooth = noteSmooth == 0 ? p.out : noteSmooth * 0.96f + p.out * 0.04f;
            viewCenter = viewCenter == 0 ? noteSmooth : viewCenter * 0.97f + noteSmooth * 0.03f;
        }
    }

    static void rr(GraphicsPath& gp, float x, float y, float w, float h, float r)
    {
        r = std::min(r, 3.f);
        gp.AddArc(x, y, r * 2, r * 2, 180, 90);
        gp.AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
        gp.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
        gp.AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
        gp.CloseFigure();
    }

    static void fillRR(Graphics& g, Brush& b, float x, float y, float w, float h, float r)
    {
        GraphicsPath gp;
        rr(gp, x, y, w, h, r);
        g.FillPath(&b, &gp);
    }

    static void strokeRR(Graphics& g, Pen& p, float x, float y, float w, float h, float r)
    {
        GraphicsPath gp;
        rr(gp, x, y, w, h, r);
        g.DrawPath(&p, &gp);
    }

    static void text(Graphics& g, const std::wstring& s, float x, float y, float size, Color c, bool bold = false, int align = 1, float spacing = 0)
    {
        FontFamily ff(L"Segoe UI");
        Font f(&ff, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
        SolidBrush br(c);
        StringFormat sf;
        sf.SetAlignment(align == 0 ? StringAlignmentNear : (align == 1 ? StringAlignmentCenter : StringAlignmentFar));
        sf.SetLineAlignment(StringAlignmentCenter);
        if (spacing <= 0) {
            g.DrawString(s.c_str(), -1, &f, PointF(x, y), &sf, &br);
        } else {
            float total = 0;
            std::vector<float> ws;
            for (wchar_t ch : s) {
                RectF r;
                wchar_t t[2] = {ch, 0};
                g.MeasureString(t, -1, &f, PointF(0, 0), &r);
                float cw = ch == L' ' ? size * 0.35f : r.Width - size * 0.28f;
                ws.push_back(cw);
                total += cw + spacing;
            }
            float cx = align == 1 ? x - total / 2 : (align == 2 ? x - total : x);
            StringFormat sn;
            sn.SetLineAlignment(StringAlignmentCenter);
            for (size_t i = 0; i < s.size(); i++) {
                wchar_t t[2] = {s[i], 0};
                g.DrawString(t, -1, &f, PointF(cx, y), &sn, &br);
                cx += ws[i] + spacing;
            }
        }
    }

    static const wchar_t* unitOf(int id)
    {
        switch (id) {
        case 1: case 19: return L" %";
        case 4: case 10: case 16: return L" Hz";
        case 5: case 11: case 12: case 13: return L" ms";
        case 8: case 9: return L" c";
        case 17: return L" dB";
        }
        return L"";
    }

    void noteName(int midi, wchar_t* out)
    {
        static const wchar_t* n[12] = {L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B"};
        int pc = ((midi % 12) + 12) % 12;
        swprintf(out, 8, L"%ls%d", n[pc], midi / 12 - 1);
    }

    bool inScale(int midi)
    {
        int st = br->scaleType();
        if (st == 2) return true;
        static const int mi[7] = {0, 2, 3, 5, 7, 8, 10}, ma[7] = {0, 2, 4, 5, 7, 9, 11};
        int rel = (((midi - br->keyRoot()) % 12) + 12) % 12;
        for (int k = 0; k < 7; k++)
            if ((st == 0 ? mi[k] : ma[k]) == rel) return true;
        return false;
    }

    void drawPitch(Graphics& g)
    {
        const float x = 24, y = 78, w = 712, h = 156;
        SolidBrush card(cCard);
        fillRR(g, card, x, y, w, h, 14);
        Pen edge(cCardEdge, 1);
        strokeRR(g, edge, x + 0.5f, y + 0.5f, w - 1, h - 1, 14);
        float center = viewCenter == 0 ? 55.f : viewCenter;
        const float span = 14.f;
        auto my = [&](float midi) { return y + h / 2 - (midi - center) / span * (h - 24); };
        GraphicsPath clip;
        rr(clip, x + 1, y + 1, w - 2, h - 2, 13);
        g.SetClip(&clip);
        int lo = (int)std::floor(center - span / 2 - 1), hi = (int)std::ceil(center + span / 2 + 1);
        for (int mn = lo; mn <= hi; mn++) {
            if (!inScale(mn)) continue;
            float yy = my((float)mn);
            bool root = (((mn - br->keyRoot()) % 12) + 12) % 12 == 0;
            Pen lp(Color(root ? 90 : 36, 17, 17, 17), 1);
            g.DrawLine(&lp, x + 54, yy, x + w - 14, yy);
            wchar_t nm[8];
            noteName(mn, nm);
            text(g, nm, x + 16, yy, 9.5f, root ? cAqua : cDim, false, 0);
        }
        int n = (int)hist.size();
        for (int pass = 0; pass < 2; pass++) {
            Color base = pass == 0 ? Color(255, 150, 150, 150) : cPink;
            float wd = pass == 0 ? 1.6f : 2.6f;
            for (int i = 1; i < n; i++) {
                float a0 = pass == 0 ? hist[i - 1].in : hist[i - 1].out;
                float a1 = pass == 0 ? hist[i].in : hist[i].out;
                if (a0 < -500 || a1 < -500 || std::fabs(a1 - a0) > 6.f) continue;
                float age = (float)i / n;
                Pen p(Color((BYTE)(40 + 215 * age), base.GetR(), base.GetG(), base.GetB()), wd);
                p.SetStartCap(LineCapRound);
                p.SetEndCap(LineCapRound);
                float px0 = x + 60 + (w - 80) * ((float)(200 - n + i - 1) / 200.f);
                float px1 = x + 60 + (w - 80) * ((float)(200 - n + i) / 200.f);
                g.DrawLine(&p, px0, my(a0), px1, my(a1));
            }
        }
        g.ResetClip();
        if (n && hist.back().out > -500) {
            int mi = (int)std::lround(hist.back().out);
            wchar_t nm[8];
            noteName(mi, nm);
            text(g, nm, x + w - 18, y + 20, 22, cText, true, 2);
        } else {
            text(g, L"listening", x + w - 18, y + 20, 13, cDim, false, 2);
        }
    }

    void drawKnob(Graphics& g, const Ctl& c, bool active)
    {
        float v = br->getNorm(c.id);
        const float a0 = 135.f, sweep = 270.f;
        Pen track(Color(50, 0, 0, 0), 3.2f);
        track.SetStartCap(LineCapRound);
        track.SetEndCap(LineCapRound);
        g.DrawArc(&track, c.x - c.r, c.y - c.r, c.r * 2, c.r * 2, a0, sweep);
        if (v > 0.002f) {
            Pen val(c.col, 3.2f);
            val.SetStartCap(LineCapRound);
            val.SetEndCap(LineCapRound);
            g.DrawArc(&val, c.x - c.r, c.y - c.r, c.r * 2, c.r * 2, a0, sweep * v);
        }
        float ir = c.r - 7;
        SolidBrush body(Color(255, active ? 206 : 236, active ? 206 : 236, active ? 206 : 236));
        g.FillEllipse(&body, c.x - ir, c.y - ir, ir * 2, ir * 2);
        Pen rim(Color(200, 17, 17, 17), 1);
        g.DrawEllipse(&rim, c.x - ir, c.y - ir, ir * 2, ir * 2);
        float ang = (a0 + sweep * v) * 3.14159265f / 180.f;
        Pen ind(c.col, 2.4f);
        ind.SetStartCap(LineCapRound);
        ind.SetEndCap(LineCapRound);
        g.DrawLine(&ind, c.x + std::cos(ang) * (ir * 0.35f), c.y + std::sin(ang) * (ir * 0.35f), c.x + std::cos(ang) * (ir * 0.88f),
                   c.y + std::sin(ang) * (ir * 0.88f));
        text(g, c.label, c.x, c.y + c.r + 12, 10, cDim, false, 1, 1.1f);
        text(g, br->text(c.id) + unitOf(c.id), c.x, c.y + c.r + 26, 11, active ? c.col : cText, false, 1);
    }

    void drawSelect(Graphics& g, const Ctl& c, bool hov)
    {
        SolidBrush bg(Color(255, hov ? 206 : 236, hov ? 206 : 236, hov ? 206 : 236));
        fillRR(g, bg, c.x, c.y, c.w, c.h, 13);
        Pen pn(Color(255, 17, 17, 17), 1);
        strokeRR(g, pn, c.x + 0.5f, c.y + 0.5f, c.w - 1, c.h - 1, 13);
        text(g, c.label, c.x + 12, c.y + c.h / 2, 9.f, cDim, false, 0, 0.9f);
        text(g, br->text(c.id), c.x + c.w - 12, c.y + c.h / 2, 12, cText, true, 2);
    }

    void paint(HDC dc)
    {
        Bitmap bmp(kW, kH, PixelFormat32bppPARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        LinearGradientBrush bg(Point(0, 0), Point(0, kH), cBg1, cBg2);
        g.FillRectangle(&bg, 0, 0, kW, kH);

        brand::drawBrand(g, L"STARZ", L"vocal");

        int pi = (int)std::lround(br->getNorm(0) * pst[0]);
        SolidBrush pill(cCard);
        fillRR(g, pill, 360, 14, 224, 36, 18);
        Pen pe(cCardEdge, 1);
        strokeRR(g, pe, 360.5f, 14.5f, 223, 35, 18);
        text(g, pnames[pi], 472, 24, 13, cText, true, 1);
        Pen chev(cAqua, 2.f);
        chev.SetStartCap(LineCapRound);
        chev.SetEndCap(LineCapRound);
        g.DrawLine(&chev, 382.f, 25.f, 378.f, 32.f);
        g.DrawLine(&chev, 378.f, 32.f, 382.f, 39.f);
        g.DrawLine(&chev, 562.f, 25.f, 566.f, 32.f);
        g.DrawLine(&chev, 566.f, 32.f, 562.f, 39.f);
        text(g, L"PRESET", 736, 24, 9.5f, cDim, false, 2, 1.4f);

        drawPitch(g);

        SolidBrush card(cCard);
        Pen edge(cCardEdge, 1);
        fillRR(g, card, 24, 250, 316, 258, 14);
        strokeRR(g, edge, 24.5f, 250.5f, 315, 257, 14);
        fillRR(g, card, 356, 250, 380, 258, 14);
        strokeRR(g, edge, 356.5f, 250.5f, 379, 257, 14);
        text(g, L"TUNER", 44, 262, 10.5f, cAqua, true, 0, 2.2f);
        text(g, L"VOICES", 376, 262, 10.5f, cPink, true, 0, 2.2f);
        text(g, L"LATENCY", 266, 420, 9.5f, cDim, false, 1, 1.4f);
        {
            wchar_t lb[32];
            swprintf(lb, 32, L"%.1f ms", br->latencyMs());
            text(g, lb, 266, 444, 20, cText, true, 1);
        }
        for (size_t i = 0; i < ctl.size(); i++) {
            const Ctl& c = ctl[i];
            bool act = (int)i == drag, hov = (int)i == hover;
            if (c.kind == kKnob) drawKnob(g, c, act || hov);
            else if (c.kind == kSelect) drawSelect(g, c, hov);
        }
        Graphics out(dc);
        out.DrawImage(&bmp, 0, 0);
    }
};

}
