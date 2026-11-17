#include <algorithm>
#include "bridge.h"
#if defined(_WIN32)
#include "fxgui.h"
#else
namespace fxgui { struct Layout { bool reverb; const wchar_t* title; const wchar_t* sub; int a; int b; }; static const int cYellow = 0, cAqua = 0, cPink = 0; }
#endif
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "base/source/fstring.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <atomic>
#include <cmath>
#include "fx.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

struct PDef {
    const char16_t* title;
    const char16_t* units;
    double mn, mx, def;
    int steps;
};

struct Preset {
    const char16_t* name;
    double v[12];
};

#if defined(ARK_REVERB)

static const FUID kPluginUID(0x4A8F1C52, 0x7B3E4D90, 0x9A21C6E5, 0x03D7B8F2);
static const char* kClassName = "Starz Reverb";
static const char* kSubCat = "Fx|Reverb";
static const bool kIsReverb = true;
static const fxgui::Layout kLayout = {true, L"STARZ", L"reverb", fxgui::cPink, fxgui::cAqua};

enum Param : ParamID { kPreset = 0, kMix, kDry, kDecay, kPre, kDamp, kLowCut, kWidth, kDuck, kMotion, kParamCount };

static const PDef kDefs[kParamCount] = {
    {u"Preset", u"", 0, 7, 0, 7},
    {u"Mix", u"%", 0, 100, 30, 0},
    {u"Dry", u"%", 0, 100, 100, 0},
    {u"Decay", u"s", 0.2, 10, 1.8, 0},
    {u"Pre-delay", u"ms", 0, 150, 20, 0},
    {u"Damp", u"Hz", 1500, 16000, 7000, 0},
    {u"Low Cut", u"Hz", 20, 800, 180, 0},
    {u"Width", u"%", 0, 100, 100, 0},
    {u"Duck", u"%", 0, 100, 50, 0},
    {u"Motion", u"%", 0, 100, 50, 0},
};

static const int kPresetCount = 8;
static const Preset kPresets[kPresetCount] = {
    {u"Klyukva Air", {0, 35, 100, 0.9, 15, 12000, 200, 100, 30, 40}},
    {u"Vocal Plate", {0, 30, 100, 1.4, 12, 9000, 220, 100, 40, 30}},
    {u"Tight Room", {0, 22, 100, 0.45, 5, 6500, 250, 80, 20, 10}},
    {u"Dark Hall", {0, 28, 100, 2.8, 30, 3500, 150, 100, 50, 50}},
    {u"Huge Bright", {0, 30, 100, 5, 45, 13000, 250, 100, 60, 60}},
    {u"Ghost", {0, 40, 100, 4, 0, 4500, 300, 100, 80, 80}},
    {u"Cathedral Dream", {0, 34, 100, 8, 60, 6000, 200, 100, 70, 90}},
    {u"Send Only", {0, 100, 0, 2, 20, 8000, 200, 100, 0, 40}},
};
static const wchar_t* gPresetNames[kPresetCount] = {L"Klyukva Air", L"Vocal Plate", L"Tight Room", L"Dark Hall", L"Huge Bright", L"Ghost", L"Cathedral Dream", L"Send Only"};

#else

static const FUID kPluginUID(0x4A8F1C52, 0x7B3E4D90, 0x9A21C6E5, 0x03D7B8F3);
static const char* kClassName = "Starz Delay";
static const char* kSubCat = "Fx|Delay";
static const bool kIsReverb = false;
static const fxgui::Layout kLayout = {false, L"STARZ", L"delay", fxgui::cPink, fxgui::cAqua};

enum Param : ParamID { kPreset = 0, kMix, kDry, kSync, kTime, kFeed, kLow, kHigh, kPing, kDuck, kWobble, kDrive, kParamCount };

static const PDef kDefs[kParamCount] = {
    {u"Preset", u"", 0, 7, 0, 7},
    {u"Mix", u"%", 0, 100, 35, 0},
    {u"Dry", u"%", 0, 100, 100, 0},
    {u"Sync", u"", 0, 5, 2, 5},
    {u"Time", u"ms", 20, 1500, 300, 0},
    {u"Feedback", u"%", 0, 95, 40, 0},
    {u"Low Cut", u"Hz", 20, 1000, 250, 0},
    {u"High Cut", u"Hz", 1500, 18000, 5500, 0},
    {u"Ping-Pong", u"%", 0, 100, 100, 0},
    {u"Duck", u"%", 0, 100, 50, 0},
    {u"Wobble", u"%", 0, 100, 0, 0},
    {u"Drive", u"%", 0, 100, 20, 0},
};

static const int kPresetCount = 8;
static const Preset kPresets[kPresetCount] = {
    {u"Eighth Ping-Pong", {0, 35, 100, 2, 300, 40, 250, 5500, 100, 50, 0, 20}},
    {u"Slapback", {0, 28, 100, 0, 95, 8, 200, 6000, 0, 20, 0, 10}},
    {u"Dark Tape", {0, 33, 100, 0, 380, 55, 300, 2800, 60, 50, 35, 45}},
    {u"Quarter Echo", {0, 30, 100, 1, 500, 35, 250, 5000, 100, 60, 0, 15}},
    {u"Long Wash", {0, 30, 100, 0, 700, 70, 350, 3500, 100, 70, 20, 30}},
    {u"Triplet Bounce", {0, 32, 100, 5, 300, 45, 250, 6000, 100, 50, 0, 20}},
    {u"Wide Doubler", {0, 25, 100, 0, 45, 15, 200, 9000, 100, 0, 50, 10}},
    {u"Send Only", {0, 100, 0, 2, 300, 45, 250, 5500, 100, 0, 0, 20}},
};
static const wchar_t* gPresetNames[kPresetCount] = {L"Eighth Ping-Pong", L"Slapback", L"Dark Tape", L"Quarter Echo", L"Long Wash", L"Triplet Bounce", L"Wide Doubler", L"Send Only"};

#endif

static double plainOf(int id, double norm) { return kDefs[id].mn + norm * (kDefs[id].mx - kDefs[id].mn); }
static double normOf(int id, double plain)
{
    double r = kDefs[id].mx - kDefs[id].mn;
    return r > 0 ? std::min(1.0, std::max(0.0, (plain - kDefs[id].mn) / r)) : 0.0;
}

class StarzFx : public SingleComponentEffect, public fxgui::Bridge
{
public:
    static FUnknown* createInstance(void*) { return static_cast<IAudioProcessor*>(new StarzFx()); }

    StarzFx()
    {
        for (int i = 0; i < kParamCount; i++) norm[i].store((float)normOf(i, kDefs[i].def));
        applyPreset(0, false);
    }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::initialize(context);
        if (r != kResultOk)
            return r;
        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        addUnit(new Unit(STR16("Starz"), 1, kRootUnitId, 0));
        ProgramList* pl = new ProgramList(String("Presets"), 0, 1);
        for (int i = 0; i < kPresetCount; i++)
            pl->addProgram(reinterpret_cast<const TChar*>(kPresets[i].name));
        addProgramList(pl);
        for (int i = 0; i < kParamCount; i++)
        {
            const PDef& d = kDefs[i];
            const TChar* title = reinterpret_cast<const TChar*>(d.title);
            const TChar* units = reinterpret_cast<const TChar*>(d.units);
            if (i == kPreset)
            {
                auto* p = new StringListParameter(title, i, nullptr,
                    ParameterInfo::kCanAutomate | ParameterInfo::kIsList | ParameterInfo::kIsProgramChange, 1);
                for (int k = 0; k < kPresetCount; k++)
                    p->appendString(reinterpret_cast<const TChar*>(kPresets[k].name));
                parameters.addParameter(p);
            }
#if !defined(ARK_REVERB)
            else if (i == kSync)
            {
                auto* p = new StringListParameter(title, i);
                p->appendString(STR16("Off"));
                p->appendString(STR16("1/4"));
                p->appendString(STR16("1/8"));
                p->appendString(STR16("1/8 dotted"));
                p->appendString(STR16("1/16"));
                p->appendString(STR16("1/8 triplet"));
                p->setNormalized(normOf(i, d.def));
                parameters.addParameter(p);
            }
#endif
            else
            {
                parameters.addParameter(new RangeParameter(title, i, units, d.mn, d.mx, d.def, d.steps, ParameterInfo::kCanAutomate));
            }
        }
        return kResultOk;
    }

    IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE
    {
#if defined(_WIN32)
        if (name && strcmp(name, ViewType::kEditor) == 0)
        {
            static double mn[kParamCount], mx[kParamCount], df[kParamCount];
            static int st[kParamCount];
            for (int i = 0; i < kParamCount; i++)
            {
                mn[i] = kDefs[i].mn;
                mx[i] = kDefs[i].mx;
                df[i] = kDefs[i].def;
                st[i] = kDefs[i].steps;
            }
            return new fxgui::View(this, kLayout, kParamCount, mn, mx, df, st, gPresetNames, kPresetCount);
        }
#else
        (void)name;
#endif
        return nullptr;
    }

    float getNorm(int id) override { return norm[id].load(); }
    double getPlain(int id) override { return plainOf(id, norm[id].load()); }

    void edit(int id, double v, int phase) override
    {
        if (phase == 0)
        {
            if (componentHandler)
                componentHandler->beginEdit(id);
        }
        else if (phase == 1)
        {
            setParamNormalized(id, v);
            if (componentHandler)
                componentHandler->performEdit(id, v);
        }
        else
        {
            if (componentHandler)
                componentHandler->endEdit(id);
        }
    }

    std::wstring text(int id) override
    {
        String128 s = {};
        getParamStringByValue(id, norm[id].load(), s);
        std::wstring r;
        for (int i = 0; i < 128 && s[i]; i++) r.push_back((wchar_t)s[i]);
        return r;
    }

    float effectiveDelayMs() override { return (float)effTime.load(); }

    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE
    {
        if (numIns == 1 && numOuts == 1 && inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
            return SingleComponentEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
        return kResultFalse;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE
    {
        sampleRate = setup.sampleRate;
        applyParams(120.0, false);
        fx.prepare(sampleRate);
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE
    {
        return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        if (state)
            fx.reset();
        return SingleComponentEffect::setActive(state);
    }

    tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::setParamNormalized(tag, value);
        if (tag < kParamCount)
        {
            norm[tag].store((float)value);
            if (tag == kPreset && !loading)
            {
                int idx = (int)std::lround(plainOf(kPreset, value));
                applyPreset(std::max(0, std::min(kPresetCount - 1, idx)), true);
            }
        }
        return r;
    }

    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE
    {
        if (data.inputParameterChanges)
        {
            int32 n = data.inputParameterChanges->getParameterCount();
            for (int32 i = 0; i < n; i++)
            {
                IParamValueQueue* q = data.inputParameterChanges->getParameterData(i);
                if (!q)
                    continue;
                int32 pts = q->getPointCount();
                if (pts <= 0)
                    continue;
                int32 off;
                ParamValue v;
                ParamID id = q->getParameterId();
                if (q->getPoint(pts - 1, off, v) == kResultTrue && id < kParamCount)
                    norm[id].store((float)v);
            }
        }
        if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples == 0)
            return kResultOk;
        double tempo = 120.0;
        bool tempoOk = false;
        if (data.processContext && (data.processContext->state & ProcessContext::kTempoValid) && data.processContext->tempo > 20.0)
        {
            tempo = data.processContext->tempo;
            tempoOk = true;
        }
        applyParams(tempo, tempoOk);
        float* const* in = data.inputs[0].channelBuffers32;
        float* const* out = data.outputs[0].channelBuffers32;
        int nch = data.inputs[0].numChannels;
        if (data.outputs[0].numChannels < 2)
            return kResultOk;
        fx.process(in[0], nch > 1 ? in[1] : in[0], out[0], out[1], data.numSamples);
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE
    {
        if (!state)
            return kResultFalse;
        IBStreamer s(state, kLittleEndian);
        int32 ver = 0, cnt = 0;
        if (!s.readInt32(ver) || !s.readInt32(cnt))
            return kResultFalse;
        loading = true;
        for (int i = 0; i < cnt && i < kParamCount; i++)
        {
            double v;
            if (!s.readDouble(v))
                break;
            setParamNormalized(i, v);
        }
        loading = false;
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE
    {
        if (!state)
            return kResultFalse;
        IBStreamer s(state, kLittleEndian);
        s.writeInt32(1);
        s.writeInt32(kParamCount);
        for (int i = 0; i < kParamCount; i++)
            s.writeDouble(norm[i].load());
        return kResultOk;
    }

private:
    double sampleRate = 44100.0;
    std::atomic<float> norm[kParamCount];
    std::atomic<double> effTime{300.0};
    bool loading = false;
#if defined(ARK_REVERB)
    ark::Reverb fx;
#else
    ark::Delay fx;
#endif

    void applyPreset(int idx, bool notify)
    {
        for (int id = 1; id < kParamCount; id++)
        {
            double nv = normOf(id, kPresets[idx].v[id]);
            norm[id].store((float)nv);
            if (notify)
            {
                SingleComponentEffect::setParamNormalized(id, nv);
                if (componentHandler)
                {
                    componentHandler->beginEdit(id);
                    componentHandler->performEdit(id, nv);
                    componentHandler->endEdit(id);
                }
            }
        }
    }

    void applyParams(double tempo, bool tempoOk)
    {
        auto pl = [&](int id) { return plainOf(id, norm[id].load()); };
#if defined(ARK_REVERB)
        ark::ReverbParams p;
        p.mix = (float)pl(kMix) / 100.f;
        p.dry = (float)pl(kDry) / 100.f;
        p.decay = (float)pl(kDecay);
        p.pre = (float)pl(kPre);
        p.damp = (float)pl(kDamp);
        p.lowCut = (float)pl(kLowCut);
        p.width = (float)pl(kWidth) / 100.f;
        p.duck = (float)pl(kDuck) / 100.f;
        p.mod = (float)pl(kMotion) / 100.f;
        fx.set(p);
#else
        ark::DelayParams p;
        p.mix = (float)pl(kMix) / 100.f;
        p.dry = (float)pl(kDry) / 100.f;
        int sync = (int)std::lround(pl(kSync));
        static const double beats[6] = {0.0, 1.0, 0.5, 0.75, 0.25, 1.0 / 3.0};
        double t = pl(kTime);
        if (sync > 0)
            t = 60000.0 / tempo * beats[std::max(0, std::min(5, sync))];
        t = std::min(2400.0, std::max(20.0, t));
        effTime.store(t);
        p.timeMs = (float)t;
        p.feed = (float)pl(kFeed) / 100.f;
        p.low = (float)pl(kLow);
        p.high = (float)pl(kHigh);
        p.ping = (float)pl(kPing) / 100.f;
        p.duck = (float)pl(kDuck) / 100.f;
        p.wobble = (float)pl(kWobble) / 100.f;
        p.drive = (float)pl(kDrive) / 100.f;
        fx.set(p);
        (void)tempoOk;
#endif
    }
};

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF("Starz", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(kPluginUID), PClassInfo::kManyInstances, kVstAudioEffectClass, kClassName, Vst::kSimpleModeSupported,
           kSubCat, "0.2.0", kVstVersionString, StarzFx::createInstance)
END_FACTORY
