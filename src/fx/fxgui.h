#pragma once
#include "ui.h"

namespace fxgui {

using namespace ui;

struct Layout {
    bool reverb;
    const wchar_t* title;
    const wchar_t* sub;
    Color accent;
    Color accent2;
};

class View : public BaseView {
public:
    View(Bridge* b, const Layout& lay, int nParams, const double* mn, const double* mx, const double* def, const int* steps,
         const wchar_t* const* presetNames, int nPresets)
        : BaseView(b, Cfg{760, 480, L"StarzFxView", lay.title, lay.sub, nParams, mn, mx, def, steps, presetNames, nPresets}), br(b), L(lay)
    {
    }

private:
    Bridge* br;
    Layout L;

    void layoutCtl() override
    {
        const float xs[6] = {92, 204, 316, 428, 540, 652};
        const float y1 = 306, y2 = 402;
        if (L.reverb) {
            knob(1, xs[0], y1, 25, L.accent, L"MIX");
            knob(2, xs[1], y1, 25, L.accent, L"DRY");
            knob(3, xs[2], y1, 25, L.accent2, L"DECAY");
            knob(4, xs[3], y1, 25, L.accent2, L"PRE-DELAY");
            knob(5, xs[4], y1, 25, L.accent2, L"DAMP");
            knob(6, xs[5], y1, 25, L.accent2, L"LOW CUT");
            knob(7, xs[0], y2, 25, cYellow, L"WIDTH");
            knob(8, xs[1], y2, 25, cYellow, L"DUCK");
            knob(9, xs[2], y2, 25, cYellow, L"MOTION");
        } else {
            knob(1, xs[0], y1, 25, L.accent, L"MIX");
            knob(2, xs[1], y1, 25, L.accent, L"DRY");
            knob(4, xs[2], y1, 25, L.accent2, L"TIME");
            knob(5, xs[3], y1, 25, L.accent2, L"FEEDBACK");
            knob(6, xs[4], y1, 25, L.accent2, L"LOW CUT");
            knob(7, xs[5], y1, 25, L.accent2, L"HIGH CUT");
            knob(8, xs[0], y2, 25, cYellow, L"PING-PONG");
            knob(9, xs[1], y2, 25, cYellow, L"DUCK");
            knob(10, xs[2], y2, 25, cYellow, L"WOBBLE");
            knob(11, xs[3], y2, 25, cYellow, L"DRIVE");
            sel(3, 508, 390, 130, cYellow, L"SYNC");
        }
    }

    const wchar_t* unitOf(int id) override
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

    void drawReverbViz(Graphics& g, float x, float y, float w, float h)
    {
        float mix = br->getNorm(1);
        float decay = (float)br->getPlain(3);
        float pre = (float)br->getPlain(4);
        float damp = (float)br->getPlain(5);
        float width = (float)br->getPlain(7) / 100.f;
        const float tmax = std::min(10.f, std::max(1.f, decay * 1.5f + pre / 1000.f));
        float px = x + 40, pw = w - 70, py = y + 18, ph2 = h - 44;
        Pen grid(Color(34, 255, 255, 255), 1);
        for (int k = 0; k <= 4; k++) {
            float yy = py + ph2 * k / 4.f;
            g.DrawLine(&grid, px, yy, px + pw, yy);
            wchar_t b[16];
            swprintf(b, 16, L"-%d", k * 20);
            text(g, b, px - 8, yy, 9, dDim, false, 2);
        }
        for (int k = 0; k <= 4; k++) {
            float xx = px + pw * k / 4.f;
            wchar_t b[16];
            swprintf(b, 16, L"%.1f s", tmax * k / 4.f);
            text(g, b, xx, py + ph2 + 12, 9, dDim, false, 1);
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
        LinearGradientBrush fillb(PointF(0, py), PointF(0, py + ph2), Color((BYTE)(50 + 100 * mix), L.accent.GetR(), L.accent.GetG(), L.accent.GetB()),
                                  Color(4, L.accent.GetR(), L.accent.GetG(), L.accent.GetB()));
        g.FillPath(&fillb, &gp);
        Pen glow(Color(40, L.accent.GetR(), L.accent.GetG(), L.accent.GetB()), 7.f);
        g.DrawLines(&glow, pts.data(), (INT)pts.size() - 2);
        Pen line(Color(255, L.accent.GetR(), L.accent.GetG(), L.accent.GetB()), 2.f);
        g.DrawLines(&line, pts.data(), (INT)pts.size() - 2);
        float tx = px + pw * std::min(tmax, decay + t0) / tmax;
        Pen mk(Color(150, 255, 255, 255), 1);
        g.DrawLine(&mk, tx, py, tx, py + ph2);
        wchar_t b[32];
        swprintf(b, 32, L"RT60 %.1f s", decay);
        text(g, b, std::min(px + pw - 4, tx + 6), py + 10, 11, dAmber, true, tx > px + pw - 90 ? 2 : 0);
        float bright = 0.25f + 0.75f * tilt;
        for (int i = 0; i < 14; i++) {
            float tt = t0 + 0.012f * (i * i * 0.37f + 1.f) + 0.004f * i;
            float amp = std::exp(-i * 0.19f) * (0.35f + 0.65f * mix);
            float bx = px + pw * tt / tmax;
            Pen er(Color((BYTE)(70 + 120 * amp), 226, 224, 216), 1.2f);
            g.DrawLine(&er, bx, py + ph2, bx, py + ph2 - ph2 * 0.55f * amp * (i % 2 ? 0.8f : 1.f) * bright);
        }
        text(g, L"DECAY", x + w - 18, y + 12, 9.5f, dDim, false, 2, 1.4f);
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
        Pen axis(Color(90, 255, 255, 255), 1);
        g.DrawLine(&axis, px, mid, px + pw, mid);
        for (int k = 0; k <= 4; k++) {
            float xx = px + pw * k / 4.f;
            wchar_t b[16];
            swprintf(b, 16, L"%d ms", (int)(tmax * k / 4.f));
            text(g, b, xx, y + h - 10, 9, dDim, false, k == 0 ? 0 : (k == 4 ? 2 : 1));
        }
        float half = h / 2 - 28;
        SolidBrush dryb(dGlass);
        fillRR(g, dryb, px - 3, mid - half * 0.92f, 6, half * 0.92f, 3);
        float tone = std::min(1.f, std::max(0.15f, (high - 1500.f) / 12000.f + 0.25f - low / 4000.f));
        float a = 0.35f + 0.65f * mix;
        for (int k = 1; k < 40; k++) {
            float t = tms * k;
            if (t > tmax) break;
            float amp = std::pow(feed, (float)(k - 1)) * a;
            if (amp < 0.01f) break;
            bool up = ping > 0.5f ? (k % 2 == 1) : true;
            Color c = ping > 0.5f ? (up ? L.accent : Color(255, 226, 224, 216)) : L.accent;
            float xx = px + pw * t / tmax;
            float hh = half * std::min(1.f, amp * 1.15f) * (0.55f + 0.45f * tone);
            float pulse = 1.f + 0.04f * std::sin(phase * 6.f - k);
            SolidBrush gl(Color(40, c.GetR(), c.GetG(), c.GetB()));
            SolidBrush b(Color(240, c.GetR(), c.GetG(), c.GetB()));
            if (up) {
                fillRR(g, gl, xx - 6, mid - hh * pulse - 3, 12, hh * pulse + 3, 4);
                fillRR(g, b, xx - 3, mid - hh * pulse, 6, hh * pulse, 3);
            } else {
                fillRR(g, gl, xx - 6, mid, 12, hh * pulse + 3, 4);
                fillRR(g, b, xx - 3, mid, 6, hh * pulse, 3);
            }
        }
        wchar_t bb[48];
        swprintf(bb, 48, L"%.0f ms", tms);
        text(g, bb, x + w - 18, y + 14, 12, dAmber, true, 2);
        text(g, L"ECHOES", x + 18, y + 12, 9.5f, dDim, false, 0, 1.4f);
        text(g, ping > 0.5f ? L"L above / R below" : L"center", x + 18, y + h - 28, 9, dDim, false, 0);
    }

    void paintBody(Graphics& g) override
    {
        display(g, 24, 78, 712, 166);
        GraphicsPath clip;
        rr(clip, 25, 79, 710, 164, 4);
        g.SetClip(&clip);
        if (L.reverb) drawReverbViz(g, 24, 78, 712, 166);
        else drawDelayViz(g, 24, 78, 712, 166);
        g.ResetClip();
        plate(g, 24, 258, 712, 204);
    }
};

}
