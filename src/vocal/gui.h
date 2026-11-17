#pragma once
#include "ui.h"

namespace gui {

using namespace ui;

class View : public BaseView {
public:
    View(Bridge* b, int nParams, const double* mn, const double* mx, const double* def, const int* steps, const wchar_t* const* presetNames)
        : BaseView(b, Cfg{760, 524, L"StarzVocalView", L"STARZ", L"VOCAL", nParams, mn, mx, def, steps, presetNames, 3}), br(b)
    {
    }

private:
    Bridge* br;
    struct Pt { float in, out; };
    std::vector<Pt> hist;
    float viewCenter = 0, noteSmooth = 0;
    float lastOutMidi = 0;

    void layoutCtl() override
    {
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

    const wchar_t* unitOf(int id) override
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

    void onTick() override
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
        display(g, x, y, w, h);
        float center = viewCenter == 0 ? 55.f : viewCenter;
        const float span = 14.f;
        auto my = [&](float midi) { return y + h / 2 - (midi - center) / span * (h - 24); };
        GraphicsPath clip;
        rr(clip, x + 1, y + 1, w - 2, h - 2, 4);
        g.SetClip(&clip);
        int lo = (int)std::floor(center - span / 2 - 1), hi = (int)std::ceil(center + span / 2 + 1);
        for (int mn = lo; mn <= hi; mn++) {
            if (!inScale(mn)) continue;
            float yy = my((float)mn);
            bool root = (((mn - br->keyRoot()) % 12) + 12) % 12 == 0;
            Pen lp(Color(root ? 80 : 34, 255, 255, 255), 1);
            g.DrawLine(&lp, x + 54, yy, x + w - 14, yy);
            wchar_t nm[8];
            noteName(mn, nm);
            text(g, nm, x + 16, yy, 9.5f, root ? dAmber : dDim, root, 0);
        }
        int n = (int)hist.size();
        for (int pass = 0; pass < 3; pass++) {
            for (int i = 1; i < n; i++) {
                float a0 = pass == 0 ? hist[i - 1].in : hist[i - 1].out;
                float a1 = pass == 0 ? hist[i].in : hist[i].out;
                if (a0 < -500 || a1 < -500 || std::fabs(a1 - a0) > 6.f) continue;
                float age = (float)i / n;
                Color base = pass == 0 ? Color(255, 160, 160, 156) : cPink;
                float wd = pass == 0 ? 1.6f : (pass == 1 ? 7.f : 2.6f);
                BYTE al = (BYTE)((pass == 1 ? 14 : 40) + (pass == 1 ? 40 : 215) * age);
                Pen p(Color(al, base.GetR(), base.GetG(), base.GetB()), wd);
                p.SetStartCap(LineCapRound);
                p.SetEndCap(LineCapRound);
                float px0 = x + 60 + (w - 80) * ((float)(200 - n + i - 1) / 200.f);
                float px1 = x + 60 + (w - 80) * ((float)(200 - n + i) / 200.f);
                float q0 = pass == 0 ? a0 : a0, q1 = a1;
                g.DrawLine(&p, px0, my(q0), px1, my(q1));
            }
        }
        g.ResetClip();
        if (n && hist.back().out > -500) {
            int mi = (int)std::lround(hist.back().out);
            wchar_t nm[8];
            noteName(mi, nm);
            text(g, nm, x + w - 18, y + 22, 24, Color(60, 255, 150, 60), true, 2);
            text(g, nm, x + w - 18, y + 21, 24, dAmber, true, 2);
        } else {
            text(g, L"listening", x + w - 18, y + 20, 13, dDim, false, 2);
        }
    }

    void paintBody(Graphics& g) override
    {
        drawPitch(g);
        plate(g, 24, 250, 316, 262);
        plate(g, 356, 250, 380, 262);
        engrave(g, L"TUNER", 44, 266, 10.5f, cText, true, 0, 2.4f);
        engrave(g, L"VOICES", 376, 266, 10.5f, Color(255, 200, 70, 0), true, 0, 2.4f);
        engrave(g, L"LATENCY", 266, 417, 9.5f, Color(255, 96, 96, 94), true, 1, 1.4f);
        wchar_t lb[32];
        swprintf(lb, 32, L"%.1f ms", br->latencyMs());
        lcd(g, 266, 442, 96, 26, lb, 15.f, dAmber);
    }
};

}
