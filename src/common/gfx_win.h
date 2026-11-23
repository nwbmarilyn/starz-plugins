#pragma once
#define NOMINMAX
#include <algorithm>
#include <windows.h>
#include <objidl.h>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <shlwapi.h>
#include <commdlg.h>

namespace Gdiplus { inline float gDeviceScale = 1.f; }

namespace gfx {

inline ULONG_PTR gToken = 0;

inline void startup()
{
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&gToken, &in, nullptr);
}

inline void shutdown() { Gdiplus::GdiplusShutdown(gToken); }

inline Gdiplus::Image* loadLogo(const unsigned char* data, unsigned long size)
{
    IStream* st = SHCreateMemStream(data, (UINT)size);
    if (!st) return nullptr;
    Gdiplus::Image* im = Gdiplus::Image::FromStream(st);
    Gdiplus::Bitmap* decoded = nullptr;
    if (im && im->GetLastStatus() == Gdiplus::Ok) {
        decoded = new Gdiplus::Bitmap(im->GetWidth(), im->GetHeight(), PixelFormat32bppPARGB);
        Gdiplus::Graphics g(decoded);
        g.DrawImage(im, 0, 0);
    }
    delete im;
    st->Release();
    return decoded;
}

}
