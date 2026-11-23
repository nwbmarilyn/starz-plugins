#pragma once

namespace ui {

class BaseView : public Steinberg::CPluginView, public ViewCore {
public:
    BaseView(Host* host, const Cfg& cfg) : ViewCore(host, cfg)
    {
        rect = Steinberg::ViewRect(0, 0, kW, kH);
    }

    ~BaseView() override;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API removed() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canResize() SMTG_OVERRIDE { return Steinberg::kResultFalse; }

protected:
    void* nsView = nullptr;
    void* nsTimer = nullptr;

    bool pickFile(bool save, userpresets::fs::path& out) override;
};

}
