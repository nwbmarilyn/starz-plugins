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

namespace fxgui {

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

struct Layout {
    bool reverb;
    const wchar_t* title;
    const wchar_t* sub;
    Color accent;
    Color accent2;
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

static const Color cBg1(255, 255, 255, 255), cBg2(255, 243, 243, 243), cCard(255, 245, 245, 245), cCardEdge(255, 17, 17, 17);
static const Color cText(255, 17, 17, 17), cDim(255, 112, 112, 112), cPink(255, 255, 85, 0), cAqua(255, 17, 17, 17),
    cYellow(255, 82, 82, 82);

class View : public Steinberg::CPluginView {
public:
    static constexpr int kW = 760, kH = 468;

    View(Bridge* b, const Layout& lay, int nParams, const double* mn, const double* mx, const double* def, const int* steps,
         const wchar_t* const* presetNames, int nPresets)
        : br(b), L(lay), np(nParams), pmn(mn), pmx(mx), pdef(def), pst(steps), pnames(presetNames), nPre(nPresets)
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
            wc.lpszClassName = L"StarzFxView";
            RegisterClassW(&wc);
            reg = true;
        }
        hwnd = CreateWindowExW(0, L"StarzFxView", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, kW, kH, (HWND)parent, nullptr, hi, this);
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
    Bridge* br;
    Layout L;
    int np;
    const double *pmn, *pmx, *pdef;
    const int* pst;
    const wchar_t* const* pnames;
    int nPre;
    HWND hwnd = nullptr;
    std::vector<Ctl> ctl;
    int drag = -1, hover = -1;
    int dragY = 0;
    float dragN = 0;
    float phase = 0;

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
        auto knob = [&](int id, float cx, float cy, Color c, const wchar_t* lab) { ctl.push_back({id, kKnob, cx, cy, 24, 0, 0, c, lab}); };
        auto sel = [&](int id, float x, float y, float w, Color c, const wchar_t* lab) { ctl.push_back({id, kSelect, x, y, 0, w, 26, c, lab}); };
        ctl.push_back({0, kArrowL, 366, 20, 0, 24, 24, cText, L""});
        ctl.push_back({0, kArrowR, 574, 20, 0, 24, 24, cText, L""});
        const float xs[6] = {92, 204, 316, 428, 540, 652};
        const float y1 = 306, y2 = 398;
        if (L.reverb) {
            knob(1, xs[0], y1, L.accent, L"MIX");
            knob(2, xs[1], y1, L.accent, L"DRY");
            knob(3, xs[2], y1, L.accent2, L"DECAY");
            knob(4, xs[3], y1, L.accent2, L"PRE-DELAY");
            knob(5, xs[4], y1, L.accent2, L"DAMP");
            knob(6, xs[5], y1, L.accent2, L"LOW CUT");
            knob(7, xs[0], y2, cYellow, L"WIDTH");
            knob(8, xs[1], y2, cYellow, L"DUCK");
            knob(9, xs[2], y2, cYellow, L"MOTION");
        } else {
            knob(1, xs[0], y1, L.accent, L"MIX");
            knob(2, xs[1], y1, L.accent, L"DRY");
            knob(4, xs[2], y1, L.accent2, L"TIME");
            knob(5, xs[3], y1, L.accent2, L"FEEDBACK");
            knob(6, xs[4], y1, L.accent2, L"LOW CUT");
            knob(7, xs[5], y1, L.accent2, L"HIGH CUT");
            knob(8, xs[0], y2, cYellow, L"PING-PONG");
            knob(9, xs[1], y2, cYellow, L"DUCK");
            knob(10, xs[2], y2, cYellow, L"WOBBLE");
            knob(11, xs[3], y2, cYellow, L"DRIVE");
            sel(3, 508, 386, 130, cYellow, L"SYNC");
        }
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
            phase += 0.033f;
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
        case WM_MOUSEMOVE: {
            int x = (short)LOWORD(l), y = (short)HIWORD(l);
            if (drag >= 0) {
                const Ctl& c = ctl[drag];
                double sens = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.0008 : 0.006;
                br->edit(c.id, quant(c.id, dragN + (dragY - y) * sens), 1);
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

    const wchar_t* unitOf(int id) const
    {
        if (L.reverb) {
            switch (id) {
            case 1: case 2: case 7: case 8: case 9: return L" %";
            case 3: return L" s";
            case 4: return L" ms";
            case 5: case 6: return L" Hz";
            }
        } else {
            switch (id) {
            case 1: case 2: case 5: case 8: case 9: case 10: case 11: return L" %";
            case 4: return L" ms";
            case 6: case 7: return L" Hz";
            }
        }
        return L"";
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
        SolidBrush body(Color(255, active ? 232 : 255, active ? 232 : 255, active ? 232 : 255));
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
        SolidBrush bg(Color(255, hov ? 232 : 255, hov ? 232 : 255, hov ? 232 : 255));
        fillRR(g, bg, c.x, c.y, c.w, c.h, 13);
        Pen pn(Color(255, 17, 17, 17), 1);
        strokeRR(g, pn, c.x + 0.5f, c.y + 0.5f, c.w - 1, c.h - 1, 13);
        text(g, c.label, c.x + 12, c.y + c.h / 2, 9.f, cDim, false, 0, 0.9f);
        text(g, br->text(c.id), c.x + c.w - 12, c.y + c.h / 2, 12, cText, true, 2);
    }

    void drawReverbViz(Graphics& g, float x, float y, float w, float h)
    {
        float mix = br->getNorm(1);
        float decay = (float)br->getPlain(3);
        float pre = (float)br->getPlain(4);
        float damp = (float)br->getPlain(5);
        float width = (float)br->getPlain(7) / 100.f;
        const float tmax = std::min(10.f, std::max(1.f, decay * 1.5f + pre / 1000.f));
        float px = x + 40, pw = w - 70, py = y + 18, ph2 = h - 44;
        Pen grid(Color(36, 0, 0, 0), 1);
        for (int k = 0; k <= 4; k++) {
            float yy = py + ph2 * k / 4.f;
            g.DrawLine(&grid, px, yy, px + pw, yy);
            wchar_t b[16];
            swprintf(b, 16, L"-%d", k * 20);
            text(g, b, px - 8, yy, 9, cDim, false, 2);
        }
        for (int k = 0; k <= 4; k++) {
            float xx = px + pw * k / 4.f;
            wchar_t b[16];
            swprintf(b, 16, L"%.1f s", tmax * k / 4.f);
            text(g, b, xx, py + ph2 + 12, 9, cDim, false, 1);
        }
        float t0 = pre / 1000.f;
        std::vector<PointF> pts;
        int n = 160;
        float tilt = std::min(1.f, std::max(0.f, (damp - 1500.f) / 14500.f));
        for (int i = 0; i <= n; i++) {
            float t = tmax * i / n;
            float db = t < t0 ? -90.f : -60.f * (t - t0) / std::max(0.1f, decay);
            float wob = (t > t0 ? 1.8f * std::sin(t * 23.f + phase * 0.8f) * std::exp(-(t - t0) * 1.3f) : 0.f) * (0.4f + 0.6f * width);
            db = std::max(-80.f, db + wob);
            pts.push_back(PointF(px + pw * t / tmax, py + ph2 * (-db / 80.f)));
        }
        pts.push_back(PointF(px + pw, py + ph2));
        pts.push_back(PointF(px, py + ph2));
        GraphicsPath gp;
        gp.AddPolygon(pts.data(), (INT)pts.size());
        LinearGradientBrush fillb(PointF(0, py), PointF(0, py + ph2), Color((BYTE)(40 + 90 * mix), L.accent.GetR(), L.accent.GetG(), L.accent.GetB()),
                                  Color(4, L.accent.GetR(), L.accent.GetG(), L.accent.GetB()));
        g.FillPath(&fillb, &gp);
        Pen line(Color(255, L.accent.GetR(), L.accent.GetG(), L.accent.GetB()), 2.f);
        g.DrawLines(&line, pts.data(), (INT)pts.size() - 2);
        float tx = px + pw * std::min(tmax, decay + t0) / tmax;
        Pen mk(Color(190, 17, 17, 17), 1);
        g.DrawLine(&mk, tx, py, tx, py + ph2);
        wchar_t b[32];
        swprintf(b, 32, L"RT60 %.1f s", decay);
        text(g, b, std::min(px + pw - 4, tx + 6), py + 10, 11, cPink, true, tx > px + pw - 90 ? 2 : 0);
        float bright = 0.25f + 0.75f * tilt;
        for (int i = 0; i < 14; i++) {
            float tt = t0 + 0.012f * (i * i * 0.37f + 1.f) + 0.004f * i;
            float amp = std::exp(-i * 0.19f) * (0.35f + 0.65f * mix);
            float bx = px + pw * tt / tmax;
            Pen er(Color((BYTE)(70 + 120 * amp), 17, 17, 17), 1.2f);
            g.DrawLine(&er, bx, py + ph2, bx, py + ph2 - ph2 * 0.55f * amp * (i % 2 ? 0.8f : 1.f) * bright);
        }
        text(g, L"DECAY", x + w - 18, y + 12, 9.5f, cDim, false, 2, 1.4f);
    }

    void drawDelayViz(Graphics& g, float x, float y, float w, float h)
    {
        float mix = br->getNorm(1);
        float feed = (float)br->getPlain(5) / 100.f;
        float tms = br->effectiveDelayMs();
        float ping = (float)br->getPlain(8) / 100.f;
        float high = (float)br->getPlain(7);
        float low = (float)br->getPlain(6);
        const float tmax = 2400.f;
        float px = x + 30, pw = w - 60, mid = y + h / 2 + 4;
        Pen axis(Color(90, 0, 0, 0), 1);
        g.DrawLine(&axis, px, mid, px + pw, mid);
        for (int k = 0; k <= 4; k++) {
            float xx = px + pw * k / 4.f;
            wchar_t b[16];
            swprintf(b, 16, L"%d ms", (int)(tmax * k / 4.f));
            text(g, b, xx, y + h - 10, 9, cDim, false, k == 0 ? 0 : (k == 4 ? 2 : 1));
        }
        float half = h / 2 - 28;
        SolidBrush dryb(L.accent2);
        fillRR(g, dryb, px - 3, mid - half * 0.92f, 6, half * 0.92f, 3);
        float tone = std::min(1.f, std::max(0.15f, (high - 1500.f) / 12000.f + 0.25f - low / 4000.f));
        float a = 0.35f + 0.65f * mix;
        for (int k = 1; k < 40; k++) {
            float t = tms * k;
            if (t > tmax) break;
            float amp = std::pow(feed, (float)(k - 1)) * a;
            if (amp < 0.01f) break;
            bool up = ping > 0.5f ? (k % 2 == 1) : true;
            Color c = ping > 0.5f ? (up ? L.accent : cAqua) : L.accent;
            float xx = px + pw * t / tmax;
            float hh = half * std::min(1.f, amp * 1.15f) * (0.55f + 0.45f * tone);
            SolidBrush b(Color(235, c.GetR(), c.GetG(), c.GetB()));
            float pulse = 1.f + 0.04f * std::sin(phase * 6.f - k);
            if (up) fillRR(g, b, xx - 3, mid - hh * pulse, 6, hh * pulse, 3);
            else fillRR(g, b, xx - 3, mid, 6, hh * pulse, 3);
        }
        wchar_t bb[48];
        swprintf(bb, 48, L"%.0f ms", tms);
        text(g, bb, x + w - 18, y + 14, 12, L.accent, true, 2);
        text(g, L"ECHOES", x + 18, y + 12, 9.5f, cDim, false, 0, 1.4f);
        text(g, ping > 0.5f ? L"L above / R below" : L"center", x + 18, y + h - 28, 9, cDim, false, 0);
    }

    void paint(HDC dc)
    {
        Bitmap bmp(kW, kH, PixelFormat32bppPARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        LinearGradientBrush bg(Point(0, 0), Point(0, kH), cBg1, cBg2);
        g.FillRectangle(&bg, 0, 0, kW, kH);
        brand::drawBrand(g, L.title, L.sub);

        int pi = (int)std::lround(br->getNorm(0) * pst[0]);
        SolidBrush pill(cCard);
        fillRR(g, pill, 360, 14, 224, 36, 18);
        Pen pe(cCardEdge, 1);
        strokeRR(g, pe, 360.5f, 14.5f, 223, 35, 18);
        text(g, pnames[std::max(0, std::min(nPre - 1, pi))], 472, 24, 13, cText, true, 1);
        Pen chev(Color(255, 17, 17, 17), 2.f);
        chev.SetStartCap(LineCapRound);
        chev.SetEndCap(LineCapRound);
        g.DrawLine(&chev, 382.f, 25.f, 378.f, 32.f);
        g.DrawLine(&chev, 378.f, 32.f, 382.f, 39.f);
        g.DrawLine(&chev, 562.f, 25.f, 566.f, 32.f);
        g.DrawLine(&chev, 566.f, 32.f, 562.f, 39.f);
        text(g, L"PRESET", 736, 24, 9.5f, cDim, false, 2, 1.4f);

        SolidBrush card(cCard);
        Pen edge(cCardEdge, 1);
        fillRR(g, card, 24, 78, 712, 166, 14);
        strokeRR(g, edge, 24.5f, 78.5f, 711, 165, 14);
        GraphicsPath clip;
        rr(clip, 25, 79, 710, 164, 13);
        g.SetClip(&clip);
        if (L.reverb) drawReverbViz(g, 24, 78, 712, 166);
        else drawDelayViz(g, 24, 78, 712, 166);
        g.ResetClip();

        fillRR(g, card, 24, 258, 712, 190, 14);
        strokeRR(g, edge, 24.5f, 258.5f, 711, 189, 14);
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
