#define NOMINMAX
#include <algorithm>
#include "bridge.h"
#include "gui.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "base/source/fstring.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <atomic>
#include <cmath>
#include "engine.h"
#include "version.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

static const FUID kPluginUID(0x4A8F1C52, 0x7B3E4D90, 0x9A21C6E5, 0x03D7B8F1);

enum Param : ParamID {
    kPreset = 0, kMix, kKey, kScale, kRef, kRetune, kTune, kVoices, kSpread, kDrift, kDriftHz,
    kFlange, kFlDepth, kDelSpread, kStereo, kStepped, kLowHz, kOutDb, kVariation, kSwarm, kLatMode, kParamCount
};

struct PDef {
    const char16_t* title;
    const char16_t* units;
    double mn, mx, def;
    int steps;
};

static const PDef kDefs[kParamCount] = {
    {u"Preset", u"", 0, 2, 0, 2},
    {u"Mix", u"%", 0, 100, 100, 0},
    {u"Key", u"", 0, 11, 7, 11},
    {u"Scale", u"", 0, 2, 0, 2},
    {u"Reference", u"Hz", 415, 466, 440, 0},
    {u"Retune Speed", u"ms", 0, 300, 20, 0},
    {u"Tune", u"", 0, 1, 1, 1},
    {u"Voices", u"", 1, 24, 1, 23},
    {u"Spread", u"c", 0, 100, 0, 0},
    {u"Drift", u"c", 0, 150, 0, 0},
    {u"Drift Rate", u"Hz", 0.1, 6, 1.2, 0},
    {u"Flange", u"ms", 0, 15, 0, 0},
    {u"Flange Depth", u"ms", 0, 4, 0, 0},
    {u"Delay Spread", u"ms", 0, 30, 0, 0},
    {u"Stereo", u"", 0, 1, 0, 1},
    {u"Stepped Voices", u"", 0, 1, 0, 1},
    {u"Lowest Voice", u"Hz", 60, 250, 110, 0},
    {u"Output", u"dB", -12, 12, 0, 0},
    {u"Variation", u"", 1, 16, 1, 15},
    {u"Swarm", u"%", 0, 100, 100, 0},
    {u"Latency Mode", u"", 0, 2, 0, 2},
};

struct Preset {
    const char16_t* name;
    double v[kParamCount];
};

static const Preset kPresets[3] = {
    {u"Auto-Tune", {0, 100, 7, 0, 440, 20, 1, 1, 0, 0, 1.2, 0, 0, 0, 0, 0, 110, 0, 1, 100}},
    {u"Auto-Tune Hard (Artist)", {1, 100, 7, 0, 440, 0, 1, 1, 0, 0, 1.2, 0, 0, 0, 0, 0, 110, 0, 1, 100}},
    {u"Mr Kitty", {2, 100, 7, 0, 440, 4, 1, 8, 25, 40, 1.2, 7, 1, 12, 0, 1, 110, 0, 1, 100}},
};

static double plainOf(int id, double norm) { return kDefs[id].mn + norm * (kDefs[id].mx - kDefs[id].mn); }
static double normOf(int id, double plain)
{
    double r = kDefs[id].mx - kDefs[id].mn;
    return r > 0 ? std::min(1.0, std::max(0.0, (plain - kDefs[id].mn) / r)) : 0.0;
}

static double latKFor(double mode) { int m = (int)std::lround(mode); return m <= 0 ? 2.0 : (m == 1 ? 1.5 : 1.0); }
static int latencyFor(double sr, double lowHz, double mode) { return (int)std::ceil(latKFor(mode) * sr / std::max(40.0, lowHz)) + 64; }

static const wchar_t* gPresetNames[3] = {L"Auto-Tune", L"Auto-Tune Hard (Artist)", L"Mr Kitty"};

class StarzVocal : public SingleComponentEffect, public gui::Bridge
{
public:
    static FUnknown* createInstance(void*) { return static_cast<IAudioProcessor*>(new StarzVocal()); }

    StarzVocal()
    {
        for (int i = 0; i < kParamCount; i++) norm[i].store((float)normOf(i, kDefs[i].def));
    }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::initialize(context);
        if (r != kResultOk)
            return r;
        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

        addUnit(new Unit(STR16("Starz"), 1, kRootUnitId, 0));

        for (int i = 0; i < kParamCount; i++)
        {
            const PDef& d = kDefs[i];
            const TChar* title = reinterpret_cast<const TChar*>(d.title);
            const TChar* units = reinterpret_cast<const TChar*>(d.units);
            if (i == kPreset)
            {
                auto* p = new StringListParameter(title, i, nullptr,
                    ParameterInfo::kCanAutomate | ParameterInfo::kIsList, 1);
                for (int k = 0; k < 3; k++)
                    p->appendString(reinterpret_cast<const TChar*>(kPresets[k].name));
                parameters.addParameter(p);
            }
            else if (i == kKey)
            {
                auto* p = new StringListParameter(title, i);
                const char16_t* names[12] = {u"C", u"C#", u"D", u"D#", u"E", u"F", u"F#", u"G", u"G#", u"A", u"A#", u"B"};
                for (int k = 0; k < 12; k++)
                    p->appendString(reinterpret_cast<const TChar*>(names[k]));
                p->setNormalized(normOf(i, d.def));
                parameters.addParameter(p);
            }
            else if (i == kScale)
            {
                auto* p = new StringListParameter(title, i);
                p->appendString(STR16("Minor"));
                p->appendString(STR16("Major"));
                p->appendString(STR16("Chromatic"));
                parameters.addParameter(p);
            }
            else if (i == kStereo)
            {
                auto* p = new StringListParameter(title, i);
                p->appendString(STR16("Wide"));
                p->appendString(STR16("L-C-R"));
                parameters.addParameter(p);
            }
            else if (i == kLatMode)
            {
                auto* p = new StringListParameter(title, i);
                p->appendString(STR16("Clean"));
                p->appendString(STR16("Balanced"));
                p->appendString(STR16("Live"));
                p->setNormalized(normOf(i, d.def));
                parameters.addParameter(p);
            }
            else if (i == kTune || i == kStepped)
            {
                auto* p = new StringListParameter(title, i);
                p->appendString(STR16("Off"));
                p->appendString(STR16("On"));
                p->setNormalized(normOf(i, d.def));
                parameters.addParameter(p);
            }
            else
            {
                parameters.addParameter(new RangeParameter(title, i, units, d.mn, d.mx, d.def, d.steps,
                                                           ParameterInfo::kCanAutomate));
            }
        }
        return kResultOk;
    }


    IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE
    {
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
            auto* view = new gui::View(this, kParamCount, mn, mx, df, st, gPresetNames);
            view->keepOwner(static_cast<IEditController*>(this));
            return view;
        }
        return nullptr;
    }

    float getNorm(int id) override { return norm[id].load(); }

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

    float pitchIn() override { return engine.guiIn(); }
    float pitchOut() override { return engine.guiOut(); }
    float refHz() override { return (float)plainOf(kRef, norm[kRef].load()); }
    float latencyMs() override { return (float)(1000.0 * latencyFor(sampleRate, plainOf(kLowHz, norm[kLowHz].load()), plainOf(kLatMode, norm[kLatMode].load())) / sampleRate); }
    int keyRoot() override { return (int)std::lround(plainOf(kKey, norm[kKey].load())); }
    int scaleType() override { return (int)std::lround(plainOf(kScale, norm[kScale].load())); }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE
    {
        if (numIns == 1 && numOuts == 1 && inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
            return SingleComponentEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
        return kResultFalse;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE
    {
        sampleRate = setup.sampleRate;
        engine.prepare(sampleRate);
        applyParams();
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE
    {
        return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        if (state)
        {
            engine.reset();
            applyParams();
        }
        return SingleComponentEffect::setActive(state);
    }

    uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE
    {
        return (uint32)latencyFor(sampleRate, plainOf(kLowHz, norm[kLowHz].load()), plainOf(kLatMode, norm[kLatMode].load()));
    }

    tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value) SMTG_OVERRIDE
    {
        if (!std::isfinite(value)) return kResultFalse;
        value = std::clamp(value, 0.0, 1.0);
        tresult r = SingleComponentEffect::setParamNormalized(tag, value);
        if (tag < kParamCount)
        {
            norm[tag].store((float)value);
            if ((tag == kLowHz || tag == kLatMode) && componentHandler)
                componentHandler->restartComponent(kLatencyChanged);
            if (tag == kPreset && !loading)
            {
                int idx = (int)std::lround(plainOf(kPreset, value));
                idx = std::max(0, std::min(2, idx));
                applyPreset(idx);
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
                if (q->getPoint(pts - 1, off, v) == kResultTrue && id < kParamCount && std::isfinite(v))
                    norm[id].store((float)std::clamp(v, 0.0, 1.0));
            }
        }
        if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples <= 0
            || data.symbolicSampleSize != kSample32 || !data.inputs || !data.outputs
            || data.inputs[0].numChannels < 1 || data.outputs[0].numChannels < 2)
            return kResultOk;
        applyParams();
        float* const* in = data.inputs[0].channelBuffers32;
        float* const* out = data.outputs[0].channelBuffers32;
        int nch = data.inputs[0].numChannels;
        if (!in || !out || !in[0] || !out[0] || !out[1])
            return kResultOk;
        engine.process(in[0], nch > 1 && in[1] ? in[1] : in[0], out[0], out[1], data.numSamples);
        data.outputs[0].silenceFlags = 0;
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
    ark::Engine engine;
    std::atomic<float> norm[kParamCount];
    bool loading = false;

    void applyPreset(int idx)
    {
        static const int effectParams[] = {kMix, kRetune, kTune, kVoices, kSpread, kDrift, kDriftHz, kFlange,
                                           kFlDepth, kDelSpread, kStereo, kStepped, kVariation, kSwarm};
        for (int id : effectParams)
        {
            double nv = normOf(id, kPresets[idx].v[id]);
            norm[id].store((float)nv);
            SingleComponentEffect::setParamNormalized(id, nv);
            if (componentHandler)
            {
                componentHandler->beginEdit(id);
                componentHandler->performEdit(id, nv);
                componentHandler->endEdit(id);
            }
        }
    }

    void applyParams()
    {
        auto pl = [&](int id) { return plainOf(id, norm[id].load()); };
        ark::Params p;
        p.refHz = (float)pl(kRef);
        p.root = (int)std::lround(pl(kKey));
        p.scaleType = (int)std::lround(pl(kScale));
        p.retuneMs = (float)pl(kRetune);
        p.tune = pl(kTune) > 0.5;
        p.preSnap = pl(kStepped) > 0.5;
        p.voices = (int)std::lround(pl(kVoices));
        p.spreadCents = (float)pl(kSpread);
        p.driftCents = (float)pl(kDrift);
        p.driftHz = (float)pl(kDriftHz);
        p.flangeMs = (float)pl(kFlange);
        p.flangeDepthMs = (float)pl(kFlDepth);
        p.delaySpreadMs = (float)pl(kDelSpread);
        p.wet = (float)pl(kMix) / 100.f;
        p.stereoMode = (int)std::lround(pl(kStereo));
        p.minF0 = (float)pl(kLowHz);
        p.outGain = (float)std::pow(10.0, pl(kOutDb) / 20.0);
        p.seed = (uint32_t)std::lround(pl(kVariation));
        p.swarm = (float)pl(kSwarm) / 100.f;
        p.latK = (float)latKFor(pl(kLatMode));
        engine.setParams(p);
   }
};

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF("Starz", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(kPluginUID), PClassInfo::kManyInstances, kVstAudioEffectClass, "Starz Vocal", Vst::kSimpleModeSupported,
           "Fx", STARZ_VERSION, kVstVersionString, StarzVocal::createInstance)
END_FACTORY
