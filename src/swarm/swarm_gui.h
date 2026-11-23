#pragma once
#include "ui.h"
#include "engine.h"

enum Param {
    kPreset, kMix, kVoices, kSpread, kDrift, kDriftRate, kFlange, kDepth, kDelay, kStereo, kParamCount
};

namespace swarm {

using namespace ui;

class View : public BaseView {
public:
    View(Host* host, const double* mn, const double* mx, const double* def, const int* steps, const wchar_t* const* names)
        : BaseView(host, Cfg{760, 458, L"StarzSwarmView", L"Starz", L"Swarm", kParamCount, mn, mx, def, steps, names, 3})
    {
    }

private:
    const Color white{255, 241, 239, 251};
    const Color muted{255, 163, 159, 187};
    const Color violet{255, 192, 168, 255};
    const Color mint{255, 130, 241, 211};

    void layoutCtl() override
    {
        knob(kMix, 76, 343, 29, mint, L"Mix");
        knob(kVoices, 163, 343, 25, violet, L"Voices");
        knob(kSpread, 250, 343, 25, violet, L"Spread");
        knob(kDrift, 337, 343, 25, violet, L"Drift");
        knob(kDriftRate, 424, 343, 25, violet, L"Rate");
        knob(kFlange, 511, 343, 25, violet, L"Flange");
        knob(kDepth, 598, 343, 25, violet, L"Depth");
        sel(kStereo, 585, 264, 141, violet, L"Stereo");
        knob(kDelay, 685, 343, 25, violet, L"Delay spread");
    }

    const wchar_t* unitOf(int id) override
    {
        switch (id) {
        case kMix: return L"%";
        case kSpread: case kDrift: return L" ct";
        case kDriftRate: return L" Hz";
        case kFlange: case kDepth: case kDelay: return L" ms";
        default: return L"";
        }
    }

    std::wstring value(int id)
    {
        if (id == kStereo) return hostp->text(id);
        double v = C.mn[id] + hostp->getNorm(id) * (C.mx[id] - C.mn[id]);
        wchar_t b[24];
        swprintf(b, 24, std::fabs(v - std::round(v)) < 0.05 ? L"%.0f" : L"%.1f", v);
        return b + std::wstring(unitOf(id));
    }

    void card(Graphics& g, float x, float y, float w, float h, Color color)
    {
        SolidBrush fill(color);
        Pen edge(Color(255, 53, 48, 71), 1.f);
        fillRR(g, fill, x, y, w, h, 4.f);
        strokeRR(g, edge, x, y, w, h, 4.f);
    }

    void paintFrame(Graphics& g) override
    {
        LinearGradientBrush bg(Point(0, 0), Point(760, 458), Color(255, 26, 22, 42), Color(255, 15, 15, 25));
        g.FillRectangle(&bg, 0, 0, kW, kH);
        text(g, L"STARZ", 29, 20, 10, mint, true, 0, 2.2f);
        text(g, L"Swarm", 27, 46, 31, white, true, 0);
        if (hostp->getNorm(0) != lastP0 && userIdx >= 0 && lastP0 >= 0.f) userIdx = -1;
        lastP0 = hostp->getNorm(0);
        card(g, 346, 15, 258, 31, Color(255, 33, 29, 49));
        text(g, presetName(), 475, 30, 14, white, true);
        if (toastUntil > nowMs()) text(g, toast, 730, 60, 11, mint, false, 2);
    }

    void paintBody(Graphics& g) override
    {
        card(g, 24, 73, 712, 175, Color(255, 26, 23, 42));
        LinearGradientBrush glow(Point(25, 74), Point(735, 247), Color(255, 38, 29, 64), Color(255, 24, 28, 47));
        g.FillRectangle(&glow, 25, 74, 710, 173);
        const float cx = 380.f;
        const float cy = 156.f;
        Pen ring(Color(67, 164, 138, 227), 1.f);
        for (int i = 1; i <= 3; ++i) {
            float w = i * 170.f;
            float h = i * 36.f;
            g.DrawEllipse(&ring, cx - w / 2, cy - h / 2, w, h);
        }
        Pen axis(Color(100, 159, 146, 199), 1.f);
        g.DrawLine(&axis, 48.f, cy, 712.f, cy);
        const int count = 1 + (int)std::lround(22.f * hostp->getNorm(kVoices));
        const float spread = hostp->getNorm(kSpread);
        const float drift = hostp->getNorm(kDrift);
        const float mix = hostp->getNorm(kMix);
        const bool lcr = hostp->getNorm(kStereo) > 0.5f;
        for (int i = 1; i <= count; ++i) {
            float side = 2.f * ark::copyPan(i, count, lcr ? 1 : 0) - 1.f;
            float x = cx + side * (250.f + 37.f * spread);
            float y = cy + (24.f + 24.f * drift) * std::sin(i * 2.4f + phase * (0.3f + hostp->getNorm(kDriftRate)));
            if (side == 0.f) x += 18.f * std::sin(i * 2.f);
            SolidBrush bloom(Color((BYTE)(16 + mix * 50), 187, 146, 255));
            SolidBrush light(Color((BYTE)(70 + mix * 185), 204, 174, 255));
            g.FillEllipse(&bloom, x - 10, y - 10.f, 20.f, 20.f);
            g.FillEllipse(&light, x - 3.5f, y - 3.5f, 7.f, 7.f);
        }
        SolidBrush mainGlow(Color(55, 103, 254, 209));
        SolidBrush mainDot(mint);
        g.FillEllipse(&mainGlow, cx - 17, cy - 17.f, 34.f, 34.f);
        g.FillEllipse(&mainDot, cx - 6, cy - 6.f, 12.f, 12.f);
        text(g, L"L", 46, 224, 11, muted);
        text(g, L"Lead", cx, 224, 12, mint);
        text(g, L"R", 714, 224, 11, muted);
        card(g, 24, 302, 712, 132, Color(255, 25, 23, 37));
    }

    void paintControl(Graphics& g, const Ctl& c, bool highlight) override
    {
        if (c.kind == kKnob) {
            float n = hostp->getNorm(c.id);
            float x = c.x - c.r, y = c.y - c.r, d = c.r * 2;
            Pen track(Color(255, 61, 54, 81), 3.f);
            Pen arc(c.col, highlight ? 4.f : 3.f);
            g.DrawArc(&track, x, y, d, d, 135.f, 270.f);
            if (n > 0.f) g.DrawArc(&arc, x, y, d, d, 135.f, 270.f * n);
            text(g, value(c.id), c.x, c.y, 14, white, true);
            text(g, c.label, c.x, 398, 13, muted);
        } else if (c.kind == kSelect) {
            card(g, c.x, c.y, c.w, c.h, highlight ? Color(255, 57, 48, 78) : Color(255, 33, 29, 49));
            text(g, c.label, c.x + 9, c.y + c.h / 2, 11, muted, false, 0);
            text(g, value(c.id), c.x + c.w - 9, c.y + c.h / 2, 12, white, true, 2);
        } else {
            const bool arrow = c.kind == kArrowL || c.kind == kArrowR;
            const wchar_t* label = c.kind == kArrowL ? L"‹" : c.kind == kArrowR ? L"›" : c.kind == kSave ? L"Save" : L"Load";
            if (!arrow || highlight) card(g, c.x + 3, 16, c.w - 6, 29, Color(255, 44, 37, 62));
            text(g, label, c.x + c.w / 2, 30, arrow ? 23.f : 12.f, white, true);
        }
    }
};

}
