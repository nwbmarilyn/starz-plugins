#pragma once

namespace ui {

class BaseView : public Steinberg::CPluginView, public ViewCore {
public:
    BaseView(Host* host, const Cfg& cfg) : ViewCore(host, cfg)
    {
        rect = Steinberg::ViewRect(0, 0, kW, kH);
    }

    ~BaseView() override { closeWindow(); }

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) SMTG_OVERRIDE
    {
        return type && strcmp(type, "HWND") == 0 ? Steinberg::kResultTrue : Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) SMTG_OVERRIDE
    {
        if (!parent || hwnd || isPlatformTypeSupported(type) != Steinberg::kResultTrue)
            return Steinberg::kResultFalse;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&BaseView::proc), &module))
            return Steinberg::kResultFalse;
        acquire();
        acquired = true;
        WNDCLASSW wc = {};
        wc.lpfnWndProc = &BaseView::proc;
        wc.hInstance = module;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = C.cls;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            closeWindow();
            return Steinberg::kResultFalse;
        }
        init();
        hwnd = CreateWindowExW(0, C.cls, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, kW, kH,
                               (HWND)parent, nullptr, module, this);
        if (!hwnd) {
            closeWindow();
            return Steinberg::kResultFalse;
        }
        SetTimer(hwnd, 1, 33, nullptr);
        return Steinberg::CPluginView::attached(parent, type);
    }

    Steinberg::tresult PLUGIN_API removed() SMTG_OVERRIDE
    {
        closeWindow();
        return Steinberg::CPluginView::removed();
    }

    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) SMTG_OVERRIDE
    {
        if (r) rect = *r;
        return Steinberg::kResultTrue;
    }
    Steinberg::tresult PLUGIN_API canResize() SMTG_OVERRIDE { return Steinberg::kResultFalse; }

protected:
    HWND hwnd = nullptr;
    HMODULE module = nullptr;
    bool acquired = false;

    void closeWindow()
    {
        if (hwnd) {
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
        if (module) {
            UnregisterClassW(C.cls, module);
            module = nullptr;
        }
        if (acquired) {
            dropCache();
            ui::release();
            acquired = false;
        }
    }

    bool pickFile(bool save, userpresets::fs::path& out) override
    {
        userpresets::ensureDir(userpresets::dirFor(C.sub));
        wchar_t buf[1024] = L"";
        if (save) lstrcpynW(buf, L"My preset.starz", 1024);
        std::wstring dir = userpresets::dirFor(C.sub).wstring();
        OPENFILENAMEW o = {};
        o.lStructSize = sizeof(o);
        o.hwndOwner = hwnd;
        o.lpstrFilter = L"Starz preset (*.starz)\0*.starz\0\0";
        o.lpstrFile = buf;
        o.nMaxFile = 1024;
        o.lpstrInitialDir = dir.c_str();
        o.lpstrDefExt = L"starz";
        o.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | (save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));
        BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
        if (!ok) return false;
        out = userpresets::fs::path(buf);
        return true;
    }

    static bool shiftDown() { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }

    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        BaseView* v;
        if (m == WM_NCCREATE) {
            v = (BaseView*)((CREATESTRUCTW*)l)->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)v);
        } else {
            v = (BaseView*)GetWindowLongPtrW(h, GWLP_USERDATA);
        }
        if (!v) return DefWindowProcW(h, m, w, l);
        return v->msg(h, m, w, l);
    }

    LRESULT msg(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        switch (m) {
        case WM_NCDESTROY:
            KillTimer(h, 1);
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            if (hwnd == h) hwnd = nullptr;
            break;
        case WM_CAPTURECHANGED:
            mouseUp();
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            tick();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            Bitmap bmp(kW, kH, PixelFormat32bppPARGB);
            Graphics g(&bmp);
            paintAll(g);
            Graphics out(dc);
            out.DrawImage(&bmp, 0, 0);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            if (mouseDown((short)LOWORD(l), (short)HIWORD(l), shiftDown())) SetCapture(h);
            return 0;
        case WM_LBUTTONDBLCLK:
            return 0;
        case WM_MOUSEMOVE:
            mouseMove((short)LOWORD(l), (short)HIWORD(l), shiftDown());
            return 0;
        case WM_LBUTTONUP:
            if (mouseUp()) ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            rightDown((short)LOWORD(l), (short)HIWORD(l));
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p = {(short)LOWORD(l), (short)HIWORD(l)};
            ScreenToClient(h, &p);
            wheel(p.x, p.y, GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1 : -1);
            return 0;
        }
        }
        return DefWindowProcW(h, m, w, l);
    }
};

}
