#define NOMINMAX
#include "ui.h"
#include "adaptive_eq.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <atomic>

using namespace Steinberg;
using namespace Steinberg::Vst;

static const FUID kPluginUID(0x4A8F1C52, 0x7B3E4D90, 0x9A21C6E5, 0x03D7B8F5);
static constexpr double kMin[] = {0}, kMax[] = {100}, kDefault[] = {50};
static constexpr int kSteps[] = {0};

struct FitHost : Host {
    virtual float gain(int band) const = 0;
    virtual bool hasBeat() const = 0;
};

class FitView : public ui::BaseView {
public:
    explicit FitView(FitHost* host)
        : BaseView(host, Cfg{570, 334, L"StarzFitView", L"Starz", L"Fit EQ", 1, kMin, kMax, kDefault, kSteps, nullptr, 0}),
          fit(host)
    {
    }

private:
    FitHost* fit;
    const ui::Color white{255, 239, 239, 253};
    const ui::Color muted{255, 165, 164, 190};
    const ui::Color mint{255, 130, 241, 211};

    void layoutCtl() override
    {
        ctl.clear();
        knob(0, 284, 252, 32, mint, L"Amount");
    }

    const wchar_t* unitOf(int) override { return L"%"; }

    void paintFrame(ui::Graphics& g) override
    {
        using namespace ui;
        LinearGradientBrush bg(Point(0, 0), Point(570, 334), Color(255, 26, 23, 45), Color(255, 14, 23, 31));
        g.FillRectangle(&bg, 0, 0, kW, kH);
        ui::text(g, L"STARZ", 26, 19, 10, mint, true, 0, 2.2f);
        ui::text(g, L"Fit EQ", 24, 43, 29, white, true, 0);
        const bool active = fit->hasBeat();
        ui::text(g, active ? L"Beat detected" : L"Waiting for beat sidechain", 542, 40, 13, active ? mint : muted, false, 2);
    }

    void paintBody(ui::Graphics& g) override
    {
        using namespace ui;
        SolidBrush panel(Color(255, 27, 32, 49));
        fillRR(g, panel, 24, 73, 522, 129, 4);
        Pen line(Color(255, 72, 77, 102), 1.f);
        g.DrawLine(&line, 44.f, 124.f, 525.f, 124.f);
        static const wchar_t* labels[] = {L"160", L"320", L"640", L"1.2k", L"2.6k", L"4.4k"};
        for (int band = 0; band < ark::AdaptiveEq::kBands; ++band) {
            float x = 72.f + band * 85.f;
            float db = fit->gain(band);
            float height = std::fabs(db) * 15.f;
            SolidBrush meter(db >= 0 ? mint : Color(255, 185, 154, 251));
            g.FillRectangle(&meter, x - 9.f, db >= 0 ? 124.f - height : 124.f, 18.f, std::max(1.f, height));
            ui::text(g, labels[band], x, 185, 11, muted);
        }
    }

    void paintControl(ui::Graphics& g, const ui::Ctl& c, bool highlight) override
    {
        using namespace ui;
        float n = hostp->getNorm(0);
        float x = c.x - c.r, y = c.y - c.r, d = c.r * 2.f;
        Pen track(Color(255, 61, 68, 84), 3.f);
        Pen arc(mint, highlight ? 4.f : 3.f);
        g.DrawArc(&track, x, y, d, d, 135.f, 270.f);
        if (n > 0.f) g.DrawArc(&arc, x, y, d, d, 135.f, 270.f * n);
        ui::text(g, hostp->text(0) + L"%", c.x, c.y, 16, white, true);
        ui::text(g, c.label, c.x, 305, 13, muted);
    }
};

class StarzFit : public SingleComponentEffect, public FitHost {
public:
    static FUnknown* createInstance(void*) { return static_cast<IAudioProcessor*>(new StarzFit()); }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        const tresult result = SingleComponentEffect::initialize(context);
        if (result != kResultOk) return result;
        addAudioInput(STR16("Vocal"), SpeakerArr::kStereo);
        sidechain = addAudioInput(STR16("Beat Sidechain"), SpeakerArr::kStereo, kAux);
        addAudioOutput(STR16("Vocal"), SpeakerArr::kStereo);
        parameters.addParameter(new RangeParameter(STR16("Amount"), 0, STR16("%"), 0, 100, 50, 0, ParameterInfo::kCanAutomate));
        return kResultOk;
    }

    IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE
    {
        if (!name || std::strcmp(name, ViewType::kEditor) != 0) return nullptr;
        auto* view = new FitView(this);
        view->keepOwner(static_cast<IEditController*>(this));
        return view;
    }

    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in, int32 ins, SpeakerArrangement* out, int32 outs) SMTG_OVERRIDE
    {
        if (ins < 1 || ins > 2 || outs != 1 || in[0] != out[0] || !supported(in[0]) || (ins == 2 && !supported(in[1])))
            return kResultFalse;
        return SingleComponentEffect::setBusArrangements(in, ins, out, outs);
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE
    {
        eq.prepare(setup.sampleRate);
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API canProcessSampleSize(int32 size) SMTG_OVERRIDE
    {
        return size == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        if (state) {
            eq.reset();
            for (auto& value : gains) value.store(0.f);
            beatDetected.store(false);
            beatHold = 0;
        }
        return SingleComponentEffect::setActive(state);
    }

    tresult PLUGIN_API setParamNormalized(ParamID id, ParamValue value) SMTG_OVERRIDE
    {
        if (id != 0 || !std::isfinite(value)) return kResultFalse;
        value = std::clamp(value, 0.0, 1.0);
        amount.store((float)value);
        return SingleComponentEffect::setParamNormalized(id, value);
    }

    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE
    {
        if (data.inputParameterChanges) {
            auto* queue = data.inputParameterChanges->getParameterData(0);
            if (queue && queue->getParameterId() == 0 && queue->getPointCount() > 0) {
                int32 offset;
                ParamValue value;
                if (queue->getPoint(queue->getPointCount() - 1, offset, value) == kResultTrue && std::isfinite(value))
                    amount.store((float)std::clamp(value, 0.0, 1.0));
            }
        }
        if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples <= 0
            || data.symbolicSampleSize != kSample32 || !data.inputs || !data.outputs
            || data.inputs[0].numChannels < 1 || data.outputs[0].numChannels < 1)
            return kResultOk;

        const auto& vocal = data.inputs[0];
        auto& out = data.outputs[0];
        const auto* const vocalBuffers = vocal.channelBuffers32;
        const auto* const outBuffers = out.channelBuffers32;
        if (!vocalBuffers || !outBuffers || !vocalBuffers[0] || !outBuffers[0]
            || (out.numChannels > 1 && !outBuffers[1]))
            return kResultOk;
        const float* beatL = nullptr;
        const float* beatR = nullptr;
        if (data.numInputs > 1 && data.inputs[1].numChannels > 0 && data.inputs[1].channelBuffers32 && sidechain->isActive()) {
            const auto& beat = data.inputs[1];
            beatL = beat.channelBuffers32[0];
            if (beat.numChannels > 1) beatR = beat.channelBuffers32[1];
        }
        bool found = false;
        for (int32 i = 0; i < data.numSamples && !found; ++i)
            found = (beatL && std::fabs(beatL[i]) > 0.001f) || (beatR && std::fabs(beatR[i]) > 0.001f);
        beatHold = found ? (int)processSetup.sampleRate : std::max(0, beatHold - data.numSamples);
        beatDetected.store(beatHold > 0, std::memory_order_relaxed);
        eq.set({amount.load(std::memory_order_relaxed)});
        eq.process(vocalBuffers[0], vocal.numChannels > 1 ? vocalBuffers[1] : nullptr, beatL, beatR,
                   outBuffers[0], out.numChannels > 1 ? outBuffers[1] : nullptr, data.numSamples);
        out.silenceFlags = 0;
        for (int i = 0; i < ark::AdaptiveEq::kBands; ++i) gains[i].store(eq.gainDb(i), std::memory_order_relaxed);
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE
    {
        if (!state) return kResultFalse;
        IBStreamer stream(state, kLittleEndian);
        return stream.writeInt32(1) && stream.writeDouble(amount.load()) ? kResultOk : kResultFalse;
    }

    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE
    {
        if (!state) return kResultFalse;
        IBStreamer stream(state, kLittleEndian);
        int32 version;
        double value;
        if (!stream.readInt32(version) || version != 1 || !stream.readDouble(value) || !std::isfinite(value))
            return kResultFalse;
        return setParamNormalized(0, std::clamp(value, 0.0, 1.0));
    }

    float getNorm(int) override { return amount.load(); }
    float gain(int band) const override { return gains[band].load(std::memory_order_relaxed); }
    bool hasBeat() const override { return beatDetected.load(std::memory_order_relaxed); }
    std::wstring text(int) override { return std::to_wstring((int)std::lround(100.f * amount.load())); }

    void edit(int, double value, int phase) override
    {
        if (phase == 1) setParamNormalized(0, value);
        if (!componentHandler) return;
        if (phase == 0) componentHandler->beginEdit(0);
        else if (phase == 1) componentHandler->performEdit(0, value);
        else componentHandler->endEdit(0);
    }

private:
    AudioBus* sidechain = nullptr;
    ark::AdaptiveEq eq;
    std::atomic<float> amount{0.5f};
    std::atomic<float> gains[ark::AdaptiveEq::kBands]{};
    std::atomic<bool> beatDetected{false};
    int beatHold = 0;

    static bool supported(SpeakerArrangement arrangement)
    {
        return arrangement == SpeakerArr::kMono || arrangement == SpeakerArr::kStereo;
    }
};

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF("Starz", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(kPluginUID), PClassInfo::kManyInstances, kVstAudioEffectClass, "Starz Fit EQ", Vst::kSimpleModeSupported,
           "Fx|EQ", STARZ_VERSION, kVstVersionString, StarzFit::createInstance)
END_FACTORY
