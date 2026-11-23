#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>
#if defined(_WIN32)
#include "gfx_win.h"
#elif defined(__APPLE__)
#include "gfx_cg.h"
#endif
#include "bridge.h"
#include "version.h"
#include "logo_data.h"
#include "userpresets.h"
#include "public.sdk/source/common/pluginview.h"

namespace ui {

using namespace Gdiplus;

static long long nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

enum Kind { kKnob, kSelect, kArrowL, kArrowR, kSave, kLoad };

struct Ctl {
    int id;
    Kind kind;
    float x, y, r;
    float w, h;
    Color col;
    std::wstring label;
};

static const Color cBg1(255, 206, 206, 203), cBg2(255, 192, 192, 190), cCard(255, 224, 224, 222), cCardEdge(255, 17, 17, 17);
static const Color cText(255, 17, 17, 17), cDim(255, 112, 112, 112), cPink(255, 255, 85, 0), cAqua(255, 17, 17, 17), cYellow(255, 82, 82, 82);
static const Color dText(255, 226, 224, 216), dDim(255, 142, 142, 138), dAmber(255, 255, 150, 60), dGlass(255, 150, 150, 146);

inline std::mutex gUiMutex;
inline int gRef = 0;
inline Image* gLogo = nullptr;

static void acquire()
{
    const std::lock_guard<std::mutex> lock(gUiMutex);
    if (gRef++ == 0) {
        gfx::startup();
        gLogo = gfx::loadLogo(kLogoPng, (unsigned long)kLogoPngSize);
    }
}

static void release()
{
    const std::lock_guard<std::mutex> lock(gUiMutex);
    if (--gRef == 0) {
        delete gLogo;
        gLogo = nullptr;
        gfx::shutdown();
    }
}

static void rr(GraphicsPath& gp, float x, float y, float w, float h, float r)
{
    r = std::min(r, 4.f);
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

static void engrave(Graphics& g, const std::wstring& s, float x, float y, float size, Color c, bool bold = false, int align = 1, float spacing = 0)
{
    text(g, s, x, y + 1.f, size, Color(150, 255, 255, 255), bold, align, spacing);
    text(g, s, x, y, size, c, bold, align, spacing);
}

static void screw(Graphics& g, float x, float y)
{
    SolidBrush sh(Color(60, 0, 0, 0));
    g.FillEllipse(&sh, x - 3.6f, y - 2.6f, 7.2f, 7.2f);
    LinearGradientBrush b(PointF(x - 4, y - 4), PointF(x + 4, y + 4), Color(255, 236, 236, 232), Color(255, 120, 120, 118));
    g.FillEllipse(&b, x - 3.6f, y - 3.6f, 7.2f, 7.2f);
    Pen rim(Color(150, 40, 40, 40), 0.8f);
    g.DrawEllipse(&rim, x - 3.6f, y - 3.6f, 7.2f, 7.2f);
    float a = (x * 0.9f + y * 1.7f);
    Pen sl(Color(210, 40, 40, 40), 1.3f);
    g.DrawLine(&sl, x - 2.4f * std::cos(a), y - 2.4f * std::sin(a), x + 2.4f * std::cos(a), y + 2.4f * std::sin(a));
}

static void plate(Graphics& g, float x, float y, float w, float h)
{
    for (int i = 4; i >= 1; i--) {
        SolidBrush sh(Color(14, 0, 0, 0));
        fillRR(g, sh, x + 1.f, y + (float)i + 1.f, w, h, 6);
    }
    LinearGradientBrush b(PointF(x, y), PointF(x, y + h), Color(255, 232, 232, 229), Color(255, 214, 214, 211));
    fillRR(g, b, x, y, w, h, 6);
    Pen edge(Color(210, 30, 30, 30), 1.f);
    strokeRR(g, edge, x + 0.5f, y + 0.5f, w - 1, h - 1, 6);
    Pen hi(Color(170, 255, 255, 255), 1.f);
    strokeRR(g, hi, x + 1.5f, y + 1.5f, w - 3, h - 3, 5);
    screw(g, x + 9, y + 9);
    screw(g, x + w - 9, y + 9);
    screw(g, x + 9, y + h - 9);
    screw(g, x + w - 9, y + h - 9);
}

static void display(Graphics& g, float x, float y, float w, float h)
{
    for (int i = 3; i >= 1; i--) {
        SolidBrush sh(Color(18, 0, 0, 0));
        fillRR(g, sh, x, y + (float)i, w, h, 5);
    }
    LinearGradientBrush b(PointF(x, y), PointF(x, y + h), Color(255, 18, 18, 20), Color(255, 40, 40, 42));
    fillRR(g, b, x, y, w, h, 5);
    GraphicsPath clip;
    rr(clip, x, y, w, h, 5);
    Region keep;
    g.GetClip(&keep);
    g.SetClip(&clip, CombineModeIntersect);
    Pen sc(Color(14, 255, 255, 255), 1.f);
    for (float yy = y + 1; yy < y + h; yy += 3.f) g.DrawLine(&sc, x, yy, x + w, yy);
    LinearGradientBrush top(PointF(x, y), PointF(x, y + 18), Color(60, 0, 0, 0), Color(0, 0, 0, 0));
    g.FillRectangle(&top, x, y, w, 18.f);
    g.SetClip(&keep);
    Pen edge(Color(255, 8, 8, 8), 1.5f);
    strokeRR(g, edge, x + 0.5f, y + 0.5f, w - 1, h - 1, 5);
    Pen rim(Color(190, 255, 255, 255), 1.f);
    strokeRR(g, rim, x - 0.5f, y - 0.5f, w + 1, h + 1, 6);
}

static void lcd(Graphics& g, float cx, float cy, float w, float h, const std::wstring& s, float size, Color col)
{
    LinearGradientBrush b(PointF(cx, cy - h / 2), PointF(cx, cy + h / 2), Color(255, 14, 14, 16), Color(255, 38, 38, 40));
    fillRR(g, b, cx - w / 2, cy - h / 2, w, h, 3);
    Pen edge(Color(255, 6, 6, 6), 1.f);
    strokeRR(g, edge, cx - w / 2 + 0.5f, cy - h / 2 + 0.5f, w - 1, h - 1, 3);
    Pen rim(Color(160, 255, 255, 255), 1.f);
    g.DrawLine(&rim, cx - w / 2 + 2, cy + h / 2 + 0.5f, cx + w / 2 - 2, cy + h / 2 + 0.5f);
    text(g, s, cx, cy, size, Color(70, col.GetR(), col.GetG(), col.GetB()), true, 1);
    text(g, s, cx, cy - 0.5f, size, col, true, 1);
}

static void metalButton(Graphics& g, float x, float y, float w, float h, bool hot)
{
    LinearGradientBrush b(PointF(x, y), PointF(x, y + h), hot ? Color(255, 250, 250, 247) : Color(255, 240, 240, 237),
                          hot ? Color(255, 214, 214, 210) : Color(255, 196, 196, 192));
    fillRR(g, b, x, y, w, h, 4);
    Pen edge(Color(235, 24, 24, 24), 1.f);
    strokeRR(g, edge, x + 0.5f, y + 0.5f, w - 1, h - 1, 4);
    Pen hi(Color(190, 255, 255, 255), 1.f);
    g.DrawLine(&hi, x + 3, y + 1.5f, x + w - 3, y + 1.5f);
}

static void analogKnob(Graphics& g, float cx, float cy, float r, float v, Color accent, bool hot, const std::wstring& label, const std::wstring& value)
{
    const float a0 = 135.f, sw = 270.f, PI = 3.14159265f;
    bool orange = accent.GetR() > 200 && accent.GetG() < 120;
    Color lit = orange ? cPink : Color(255, 30, 30, 30);
    for (int k = 0; k <= 20; k++) {
        float t = k / 20.f;
        float a = (a0 + sw * t) * PI / 180.f;
        float c = std::cos(a), s = std::sin(a);
        bool major = k % 2 == 0;
        bool on = v > 0.002f && t <= v + 0.001f;
        Pen p(on ? lit : Color(255, 128, 128, 124), major ? 1.7f : 1.f);
        float r0 = r + 1.5f, r1 = r + (major ? 6.f : 4.2f);
        g.DrawLine(&p, cx + c * r0, cy + s * r0, cx + c * r1, cy + s * r1);
    }
    const float R = r - 2.5f;
    for (int i = 4; i >= 1; i--) {
        SolidBrush sh(Color(20, 0, 0, 0));
        float rr2 = R + i * 0.9f;
        g.FillEllipse(&sh, cx - rr2 + 1.2f, cy - rr2 + 3.2f, rr2 * 2, rr2 * 2);
    }
    const int N = 24;
    std::vector<PointF> pts;
    float Rin = R - 1.9f;
    for (int k = 0; k < N; k++) {
        float step = 2.f * PI / N, a = k * step;
        pts.push_back(PointF(cx + std::cos(a) * Rin, cy + std::sin(a) * Rin));
        pts.push_back(PointF(cx + std::cos(a + 0.15f * step) * R, cy + std::sin(a + 0.15f * step) * R));
        pts.push_back(PointF(cx + std::cos(a + 0.42f * step) * R, cy + std::sin(a + 0.42f * step) * R));
        pts.push_back(PointF(cx + std::cos(a + 0.57f * step) * Rin, cy + std::sin(a + 0.57f * step) * Rin));
    }
    LinearGradientBrush body(PointF(cx - R, cy - R), PointF(cx + R, cy + R), Color(255, 98, 98, 102), Color(255, 22, 22, 24));
    g.FillPolygon(&body, pts.data(), (INT)pts.size());
    Pen outline(Color(255, 8, 8, 8), 0.9f);
    g.DrawPolygon(&outline, pts.data(), (INT)pts.size());
    float Rc = R - 3.4f;
    GraphicsPath cap;
    cap.AddEllipse(cx - Rc, cy - Rc, Rc * 2, Rc * 2);
    PathGradientBrush pg(&cap);
    pg.SetCenterPoint(PointF(cx - Rc * 0.3f, cy - Rc * 0.38f));
    pg.SetCenterColor(hot ? Color(255, 138, 138, 142) : Color(255, 112, 112, 116));
    Color sur[1] = {Color(255, 30, 30, 33)};
    int cnt = 1;
    pg.SetSurroundColors(sur, &cnt);
    g.FillPath(&pg, &cap);
    Pen ring(Color(70, 0, 0, 0), 0.8f);
    g.DrawEllipse(&ring, cx - Rc * 0.72f, cy - Rc * 0.72f, Rc * 1.44f, Rc * 1.44f);
    g.DrawEllipse(&ring, cx - Rc * 0.46f, cy - Rc * 0.46f, Rc * 0.92f, Rc * 0.92f);
    Pen edgeHi(Color(120, 255, 255, 255), 1.1f);
    g.DrawArc(&edgeHi, cx - Rc + 0.5f, cy - Rc + 0.5f, Rc * 2 - 1, Rc * 2 - 1, 195, 80);
    Pen edgeLo(Color(150, 0, 0, 0), 1.1f);
    g.DrawArc(&edgeLo, cx - Rc + 0.5f, cy - Rc + 0.5f, Rc * 2 - 1, Rc * 2 - 1, 15, 80);
    float ang = (a0 + sw * v) * PI / 180.f;
    Pen ptr(orange ? cPink : Color(255, 238, 232, 214), 2.6f);
    ptr.SetStartCap(LineCapRound);
    ptr.SetEndCap(LineCapRound);
    g.DrawLine(&ptr, cx + std::cos(ang) * Rc * 0.34f, cy + std::sin(ang) * Rc * 0.34f, cx + std::cos(ang) * Rc * 0.9f, cy + std::sin(ang) * Rc * 0.9f);
    SolidBrush spec(Color(46, 255, 255, 255));
    g.FillEllipse(&spec, cx - Rc * 0.58f, cy - Rc * 0.8f, Rc * 0.8f, Rc * 0.36f);
    engrave(g, label, cx, cy + r + 12, 9.5f, Color(255, 70, 70, 68), true, 1, 1.f);
    lcd(g, cx, cy + r + 26, 66, 15, value, 10.5f, hot ? Color(255, 255, 196, 120) : dAmber);
}

class ViewCore {
public:
    struct Cfg {
        int w, h;
        const wchar_t* cls;
        const wchar_t* title;
        const wchar_t* sub;
        int np;
        const double *mn, *mx, *def;
        const int* st;
        const wchar_t* const* pnames;
        int nPre;
    };

    ViewCore(Host* host, const Cfg& cfg) : hostp(host), C(cfg), kW(cfg.w), kH(cfg.h) {}
    virtual ~ViewCore() {}

    void keepOwner(Steinberg::FUnknown* owner) { viewOwner = owner; }

    int width() const { return kW; }
    int height() const { return kH; }

    void init()
    {
        buildBar();
        layoutCtl();
        refreshUsers();
    }

    void dropCache()
    {
        delete bgCache;
        bgCache = nullptr;
    }

    void tick()
    {
        phase += 0.033f;
        onTick();
    }

    void paintAll(Graphics& g)
    {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        paintFrame(g);
        paintBody(g);
        for (size_t i = 0; i < ctl.size(); i++) {
            const Ctl& c = ctl[i];
            bool act = (int)i == drag, hov = (int)i == hover;
            paintControl(g, c, act || hov);
        }
    }

    bool mouseDown(int x, int y, bool shift)
    {
        int i = hit(x, y);
        if (i < 0) return false;
        const Ctl c = ctl[i];
        if (c.kind == kArrowL || c.kind == kArrowR) {
            stepPreset(c.kind == kArrowL ? -1 : 1);
        } else if (c.kind == kSave) {
            doSave();
        } else if (c.kind == kLoad) {
            doLoad();
        } else if (c.kind == kSelect) {
            cycle(c.id, shift ? -1 : 1);
        } else {
            drag = i;
            dragY = y;
            dragN = hostp->getNorm(c.id);
            hostp->edit(c.id, dragN, 0);
            return true;
        }
        return false;
    }

    void mouseMove(int x, int y, bool shift)
    {
        if (drag >= 0) {
            const Ctl& c = ctl[drag];
            double sens = shift ? 0.0008 : 0.006;
            hostp->edit(c.id, quant(c.id, dragN + (dragY - y) * sens), 1);
        } else {
            hover = hit(x, y);
        }
    }

    void mouseLeave() { if (drag < 0) hover = -1; }

    bool mouseUp()
    {
        if (drag < 0) return false;
        hostp->edit(ctl[drag].id, hostp->getNorm(ctl[drag].id), 2);
        drag = -1;
        return true;
    }

    void rightDown(int x, int y)
    {
        int i = hit(x, y);
        if (i >= 0 && ctl[i].kind == kKnob) {
            int id = ctl[i].id;
            hostp->edit(id, 0, 0);
            hostp->edit(id, quant(id, (C.def[id] - C.mn[id]) / (C.mx[id] - C.mn[id])), 1);
            hostp->edit(id, 0, 2);
        }
    }

    void wheel(int x, int y, int dir)
    {
        int i = hit(x, y);
        if (i < 0) return;
        if (ctl[i].kind == kKnob) {
            int id = ctl[i].id;
            double step = C.st[id] > 0 ? 1.0 / C.st[id] : 0.02;
            hostp->edit(id, 0, 0);
            hostp->edit(id, quant(id, hostp->getNorm(id) + dir * step), 1);
            hostp->edit(id, 0, 2);
        } else if (ctl[i].kind == kSelect) {
            cycle(ctl[i].id, dir);
        } else if (ctl[i].kind == kArrowL || ctl[i].kind == kArrowR) {
            stepPreset(-dir);
        }
    }

protected:
    Steinberg::IPtr<Steinberg::FUnknown> viewOwner;
    Host* hostp;
    Cfg C;
    int kW, kH;
    std::vector<Ctl> ctl;
    int drag = -1, hover = -1;
    int dragY = 0;
    float dragN = 0;
    float phase = 0;
    Bitmap* bgCache = nullptr;
    float bgScale = 1.f;
    std::vector<userpresets::fs::path> users;
    int userIdx = -1;
    float lastP0 = -1.f;
    std::wstring toast;
    long long toastUntil = 0;

    virtual bool pickFile(bool save, userpresets::fs::path& out) = 0;
    virtual void layoutCtl() = 0;
    virtual void paintBody(Graphics& g) = 0;
    virtual const wchar_t* unitOf(int id) = 0;
    virtual void onTick() {}

    virtual void paintFrame(Graphics& g)
    {
        if (bgCache && bgScale != gDeviceScale) dropCache();
        if (!bgCache) buildBackground();
        g.DrawImage(bgCache, 0, 0);
        drawHeader(g);
    }

    virtual void paintControl(Graphics& g, const Ctl& c, bool highlight)
    {
        if (c.kind == kKnob)
            analogKnob(g, c.x, c.y, c.r, hostp->getNorm(c.id), c.col, highlight, c.label, hostp->text(c.id) + unitOf(c.id));
        else if (c.kind == kSelect)
            drawSelect(g, c, highlight);
    }

    void knob(int id, float cx, float cy, float r, Color c, const wchar_t* lab) { ctl.push_back({id, kKnob, cx, cy, r, 0, 0, c, lab}); }
    void sel(int id, float x, float y, float w, Color c, const wchar_t* lab) { ctl.push_back({id, kSelect, x, y, 0, w, 26, c, lab}); }

    void buildBar()
    {
        ctl.clear();
        ctl.push_back({0, kArrowL, 346, 10, 0, 34, 40, cText, L""});
        ctl.push_back({0, kArrowR, 570, 10, 0, 34, 40, cText, L""});
        ctl.push_back({0, kSave, 606, 16, 0, 60, 28, cText, L"SAVE"});
        ctl.push_back({0, kLoad, 674, 16, 0, 62, 28, cText, L"LOAD"});
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
        if (C.st[id] > 0) n = std::round(n * C.st[id]) / C.st[id];
        return n;
    }

    void cycle(int id, int dir)
    {
        int n = C.st[id];
        if (n <= 0) return;
        int idx = (int)std::lround(hostp->getNorm(id) * n);
        idx = ((idx + dir) % (n + 1) + (n + 1)) % (n + 1);
        hostp->edit(id, 0, 0);
        hostp->edit(id, (double)idx / n, 1);
        hostp->edit(id, 0, 2);
    }

    int factoryIdx()
    {
        int i = (int)std::lround(hostp->getNorm(0) * C.st[0]);
        return std::max(0, std::min(C.nPre - 1, i));
    }

    void refreshUsers() { users = userpresets::list(C.sub); }

    void applyValues(const std::vector<double>& v)
    {
        for (int id = 1; id < C.np && id < (int)v.size(); id++) {
            hostp->edit(id, 0, 0);
            hostp->edit(id, quant(id, v[id]), 1);
            hostp->edit(id, 0, 2);
        }
    }

    bool applyFile(const userpresets::fs::path& p)
    {
        std::vector<double> v;
        if (!userpresets::load(p, v)) return false;
        applyValues(v);
        lastP0 = hostp->getNorm(0);
        return true;
    }

    void stepPreset(int dir)
    {
        int total = C.nPre + (int)users.size();
        int cur = userIdx >= 0 ? C.nPre + userIdx : factoryIdx();
        int nx = ((cur + dir) % total + total) % total;
        if (nx < C.nPre) {
            userIdx = -1;
            hostp->edit(0, 0, 0);
            hostp->edit(0, (double)nx / C.st[0], 1);
            hostp->edit(0, 0, 2);
        } else {
            userIdx = nx - C.nPre;
            if (!applyFile(users[userIdx])) userIdx = -1;
        }
        lastP0 = hostp->getNorm(0);
    }

    void doSave()
    {
        userpresets::fs::path p;
        if (!pickFile(true, p)) return;
        std::vector<double> v((size_t)C.np);
        for (int i = 0; i < C.np; i++) v[i] = hostp->getNorm(i);
        if (userpresets::save(p, v)) {
            refreshUsers();
            userIdx = -1;
            for (size_t i = 0; i < users.size(); i++)
                if (users[i].filename() == p.filename()) userIdx = (int)i;
            lastP0 = hostp->getNorm(0);
            toast = L"saved " + p.stem().wstring();
        } else {
            toast = L"save failed";
        }
        toastUntil = nowMs() + 2600;
    }

    void doLoad()
    {
        userpresets::fs::path p;
        if (!pickFile(false, p)) return;
        if (applyFile(p)) {
            refreshUsers();
            userIdx = -1;
            for (size_t i = 0; i < users.size(); i++)
                if (users[i].filename() == p.filename()) userIdx = (int)i;
            if (userIdx < 0) {
                users.push_back(p);
                userIdx = (int)users.size() - 1;
            }
            toast = L"loaded " + p.stem().wstring();
        } else {
            toast = L"load failed";
        }
        toastUntil = nowMs() + 2600;
    }

    std::wstring presetName()
    {
        if (userIdx >= 0 && userIdx < (int)users.size()) return users[userIdx].stem().wstring();
        return C.pnames[factoryIdx()];
    }

    void drawSelect(Graphics& g, const Ctl& c, bool hov)
    {
        metalButton(g, c.x, c.y, c.w, c.h, hov);
        text(g, c.label, c.x + 10, c.y + c.h / 2, 9.f, Color(255, 96, 96, 94), true, 0, 0.9f);
        text(g, hostp->text(c.id), c.x + c.w - 10, c.y + c.h / 2, 12, cText, true, 2);
    }

    void buildBackground()
    {
        delete bgCache;
        bgCache = new Bitmap(kW, kH, PixelFormat32bppPARGB);
        bgScale = gDeviceScale;
        Graphics b(bgCache);
        LinearGradientBrush bg(Gdiplus::Point(0, 0), Gdiplus::Point(0, kH), cBg1, cBg2);
        b.FillRectangle(&bg, 0, 0, kW, kH);
        Pen grain(Color(9, 0, 0, 0), 1.f);
        for (int y = 0; y < kH; y += 2) b.DrawLine(&grain, 0, y, kW, y);
        Pen grain2(Color(7, 255, 255, 255), 1.f);
        for (int y = 1; y < kH; y += 4) b.DrawLine(&grain2, 0, y, kW, y);
        GraphicsPath vg;
        vg.AddRectangle(Gdiplus::Rect(-kW / 3, -kH / 3, kW + 2 * (kW / 3), kH + 2 * (kH / 3)));
        PathGradientBrush pg(&vg);
        pg.SetCenterColor(Color(0, 0, 0, 0));
        Color sur[1] = {Color(46, 0, 0, 0)};
        int cnt = 1;
        pg.SetSurroundColors(sur, &cnt);
        pg.SetFocusScales(0.55f, 0.55f);
        b.FillRectangle(&pg, 0, 0, kW, kH);
        Pen frame(Color(210, 30, 30, 30), 1.f);
        b.DrawRectangle(&frame, 0, 0, kW - 1, kH - 1);
        Pen fh(Color(150, 255, 255, 255), 1.f);
        b.DrawRectangle(&fh, 1, 1, kW - 3, kH - 3);
    }

    void drawHeader(Graphics& g)
    {
        if (gLogo) {
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            for (int i = 3; i >= 1; i--) {
                SolidBrush sh(Color(22, 0, 0, 0));
                g.FillEllipse(&sh, 22.f - (float)i + 1.f, 8.f + (float)i + 1.f, 44.f + 2.f * (float)i - 2.f, 44.f + 2.f * (float)i - 2.f);
            }
            g.DrawImage(gLogo, RectF(20.f, 6.f, 48.f, 48.f));
        }
        std::wstring subU = C.sub;
        for (auto& ch : subU) ch = (wchar_t)towupper(ch);
        text(g, C.title, 78, 22, 20, cText, true, 0, 2.6f);
        text(g, subU, 78, 41, 11, Color(255, 96, 96, 94), true, 0, 1.8f);
        text(g, STARZ_VERSION_TEXT, 78 + 14.f + 8.6f * (float)subU.size(), 41, 10, cDim, false, 0);

        if (hostp->getNorm(0) != lastP0 && userIdx >= 0 && lastP0 >= 0.f) userIdx = -1;

        const Ctl* aL = nullptr;
        const Ctl* aR = nullptr;
        const Ctl* sv = nullptr;
        const Ctl* ld = nullptr;
        for (size_t i = 0; i < ctl.size(); i++) {
            if (ctl[i].kind == kArrowL) aL = &ctl[i];
            if (ctl[i].kind == kArrowR) aR = &ctl[i];
            if (ctl[i].kind == kSave) sv = &ctl[i];
            if (ctl[i].kind == kLoad) ld = &ctl[i];
        }
        auto hovered = [&](const Ctl* c) { return hover >= 0 && &ctl[hover] == c; };
        float by = 16.f, bh = 28.f;
        metalButton(g, 350.f, by, 28.f, bh, hovered(aL));
        metalButton(g, 572.f, by, 28.f, bh, hovered(aR));
        Pen chev(Color(255, 17, 17, 17), 2.f);
        chev.SetStartCap(LineCapRound);
        chev.SetEndCap(LineCapRound);
        g.DrawLine(&chev, 368.f, by + 8.f, 361.f, by + 14.f);
        g.DrawLine(&chev, 361.f, by + 14.f, 368.f, by + 20.f);
        g.DrawLine(&chev, 582.f, by + 8.f, 589.f, by + 14.f);
        g.DrawLine(&chev, 589.f, by + 14.f, 582.f, by + 20.f);
        lcd(g, 475.f, by + bh / 2, 186.f, bh, presetName(), 13.f, dAmber);
        metalButton(g, sv->x, by, sv->w, bh, hovered(sv));
        metalButton(g, ld->x, by, ld->w, bh, hovered(ld));
        engrave(g, L"SAVE", sv->x + sv->w / 2, by + bh / 2, 10.5f, cText, true, 1, 1.4f);
        engrave(g, L"LOAD", ld->x + ld->w / 2, by + bh / 2, 10.5f, cText, true, 1, 1.4f);
        if (toastUntil > nowMs()) text(g, toast, 736, 58, 10.5f, Color(255, 200, 70, 0), true, 2);
    }
};

}

#if defined(_WIN32)
#include "view_win.h"
#elif defined(__APPLE__)
#include "view_mac.h"
#endif
