#define NOMINMAX
#include <algorithm>
#include "swarm_gui.h"
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

static const FUID kPluginUID(0x4A8F1C52, 0x7B3E4D90, 0x9A21C6E5, 0x03D7B8F4);

struct PDef {
    const char16_t* title;
    const char16_t* units;
    double mn, mx, def;
    int steps;
};

static constexpr PDef kDefs[kParamCount] = {
    {u"Preset", u"", 0, 2, 0, 2},
    {u"Mix", u"%", 0, 100, 40, 0},
    {u"Voices", u"", 1, 23, 7, 22},
    {u"Spread", u"cents", 0, 100, 25, 0},
    {u"Drift", u"cents", 0, 150, 40, 0},
    {u"Drift Rate", u"Hz", 0.1, 6, 1.2, 0},
    {u"Flange", u"ms", 0, 15, 7, 0},
    {u"Depth", u"ms", 0, 4, 1, 0},
    {u"Delay Spread", u"ms", 0, 30, 12, 0},
    {u"Stereo", u"", 0, 1, 0, 1},
};

static constexpr double kPresets[3][kParamCount] = {
    {0, 40, 7, 25, 40, 1.2, 7, 1, 12, 0},
    {1, 25, 3, 14, 16, 0.8, 3, 0.5, 5, 1},
    {2, 48, 11, 42, 58, 1.7, 8, 2, 22, 0},
};

static double plainOf(int id, double norm) { return kDefs[id].mn + norm * (kDefs[id].mx - kDefs[id].mn); }
static double normOf(int id, double plain)
{
    double r = kDefs[id].mx - kDefs[id].mn;
    return r > 0 ? std::min(1.0, std::max(0.0, (plain - kDefs[id].mn) / r)) : 0.0;
}

static const wchar_t* kPresetNames[] = {L"Mr Kitty", L"Soft Doubles", L"Wide Bloom"};

class StarzSwarm : public SingleComponentEffect, public Host
{
public:
    static FUnknown* createInstance(void*) { return static_cast<IAudioProcessor*>(new StarzSwarm()); }

    StarzSwarm()
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
                    p->appendString(k == 0 ? STR16("Mr Kitty") : k == 1 ? STR16("Soft Doubles") : STR16("Wide Bloom"));
                parameters.addParameter(p);
            }
            else if (i == kStereo)
            {
                auto* p = new StringListParameter(title, i);
                p->appendString(STR16("Wide"));
                p->appendString(STR16("L-C-R"));
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
            auto* view = new swarm::View(this, mn, mx, df, st, kPresetNames);
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
            applyParams();
            engine.reset();
        }
        return SingleComponentEffect::setActive(state);
    }

    uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE
    {
        return (uint32)std::ceil(2.0 * sampleRate / 110.0) + 64;
    }

    tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value) SMTG_OVERRIDE
    {
        if (!std::isfinite(value)) return kResultFalse;
        value = std::clamp(value, 0.0, 1.0);
        tresult r = SingleComponentEffect::setParamNormalized(tag, value);
        if (tag < kParamCount)
        {
            norm[tag].store((float)value);
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
        for (int id = 1; id < kParamCount; ++id)
        {
            double nv = normOf(id, kPresets[idx][id]);
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
        p.tune = false;
        p.copiesOnly = true;
        p.voices = 1 + (int)std::lround(pl(kVoices));
        p.spreadCents = (float)pl(kSpread);
        p.driftCents = (float)pl(kDrift);
        p.driftHz = (float)pl(kDriftRate);
        p.flangeMs = (float)pl(kFlange);
        p.flangeDepthMs = (float)pl(kDepth);
        p.delaySpreadMs = (float)pl(kDelay);
        p.wet = (float)pl(kMix) / 100.f;
        p.stereoMode = (int)std::lround(pl(kStereo));
        p.minF0 = 110.f;
        p.latK = 2.f;
        engine.setParams(p);
   }
};

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF("Starz", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(kPluginUID), PClassInfo::kManyInstances, kVstAudioEffectClass, "Starz Swarm", Vst::kSimpleModeSupported,
           "Fx", STARZ_VERSION, kVstVersionString, StarzSwarm::createInstance)
END_FACTORY
