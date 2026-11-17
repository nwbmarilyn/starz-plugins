#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;

int main()
{
    GdiplusStartupInput in;
    ULONG_PTR tok;
    GdiplusStartup(&tok, &in, nullptr);
    Bitmap bmp(64, 64, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush b(Color(255, 200, 120, 140));
    g.FillEllipse(&b, 4, 4, 40, 40);
    FontFamily ff(L"Segoe UI");
    Font f(&ff, 12, FontStyleBold, UnitPixel);
    g.DrawString(L"Hi", -1, &f, PointF(10, 10), &b);
    GdiplusShutdown(tok);
    return 0;
}
