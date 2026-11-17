#pragma once
#include <string>
#include "version.h"

namespace brand {

static void drawBrand(Gdiplus::Graphics& g, const wchar_t* title, const wchar_t* sub)
{
    using namespace Gdiplus;
    SolidBrush white(Color(255, 255, 255, 255)), black(Color(255, 17, 17, 17)), gray(Color(255, 190, 190, 190));
    FontFamily ff(L"Segoe UI");
    Font f1(&ff, 16, FontStyleBold, UnitPixel);
    Font f2(&ff, 11, FontStyleBold, UnitPixel);
    StringFormat sf;
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF r1, r2;
    g.MeasureString(title, -1, &f1, PointF(0, 0), &r1);
    g.MeasureString(sub, -1, &f2, PointF(0, 0), &r2);
    g.FillRectangle(&black, 24.f, 12.f, r1.Width + 8.f, 26.f);
    g.DrawString(title, -1, &f1, PointF(28.f, 25.f), &sf, &white);
    g.FillRectangle(&black, 24.f, 40.f, r2.Width + 8.f, 16.f);
    g.DrawString(sub, -1, &f2, PointF(28.f, 48.f), &sf, &gray);
    Font f3(&ff, 11, FontStyleRegular, UnitPixel);
    SolidBrush dim(Color(255, 70, 70, 70));
    g.DrawString(STARZ_VERSION_TEXT, -1, &f3, PointF(40.f + r2.Width, 48.f), &sf, &dim);
}

}
