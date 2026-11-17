#pragma once
#include <string>

namespace brand {

static void drawBrand(Gdiplus::Graphics& g, const wchar_t* title, const wchar_t* sub)
{
    using namespace Gdiplus;
    SolidBrush white(Color(255, 255, 255, 255)), black(Color(255, 17, 17, 17)), gray(Color(255, 190, 190, 190));
    g.FillRectangle(&white, 24, 12, 44, 44);
    Pen edge(Color(255, 17, 17, 17), 1.5f);
    g.DrawRectangle(&edge, 24.f, 12.f, 44.f, 44.f);
    PointF star[8] = {PointF(46, 17), PointF(49.5f, 31), PointF(63, 34), PointF(49.5f, 37), PointF(46, 51),
                      PointF(42.5f, 37), PointF(29, 34), PointF(42.5f, 31)};
    g.FillPolygon(&black, star, 8);
    FontFamily ff(L"Segoe UI");
    Font f1(&ff, 16, FontStyleBold, UnitPixel);
    Font f2(&ff, 11, FontStyleBold, UnitPixel);
    StringFormat sf;
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF r1, r2;
    g.MeasureString(title, -1, &f1, PointF(0, 0), &r1);
    g.MeasureString(sub, -1, &f2, PointF(0, 0), &r2);
    g.FillRectangle(&black, 80.f, 12.f, r1.Width + 8.f, 26.f);
    g.DrawString(title, -1, &f1, PointF(84.f, 25.f), &sf, &white);
    g.FillRectangle(&black, 80.f, 40.f, r2.Width + 8.f, 16.f);
    g.DrawString(sub, -1, &f2, PointF(84.f, 48.f), &sf, &gray);
}

}