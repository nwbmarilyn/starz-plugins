#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

typedef unsigned char BYTE;
typedef int INT;

namespace Gdiplus {

enum SmoothingMode { SmoothingModeAntiAlias };
enum TextRenderingHint { TextRenderingHintAntiAliasGridFit };
enum InterpolationMode { InterpolationModeHighQualityBicubic };
enum LineCap { LineCapFlat, LineCapRound };
enum FontStyle { FontStyleRegular = 0, FontStyleBold = 1 };
enum Unit { UnitPixel };
enum StringAlignment { StringAlignmentNear, StringAlignmentCenter, StringAlignmentFar };
enum CombineMode { CombineModeReplace, CombineModeIntersect };
enum PixelFormat { PixelFormat32bppPARGB };

inline float gDeviceScale = 1.f;

struct Color {
    BYTE a, r, g, b;
    Color() : a(255), r(0), g(0), b(0) {}
    Color(BYTE r_, BYTE g_, BYTE b_) : a(255), r(r_), g(g_), b(b_) {}
    Color(BYTE a_, BYTE r_, BYTE g_, BYTE b_) : a(a_), r(r_), g(g_), b(b_) {}
    BYTE GetA() const { return a; }
    BYTE GetR() const { return r; }
    BYTE GetG() const { return g; }
    BYTE GetB() const { return b; }
};

struct PointF {
    float X, Y;
    PointF() : X(0), Y(0) {}
    PointF(float x, float y) : X(x), Y(y) {}
};

struct Point {
    int X, Y;
    Point() : X(0), Y(0) {}
    Point(int x, int y) : X(x), Y(y) {}
};

struct RectF {
    float X, Y, Width, Height;
    RectF() : X(0), Y(0), Width(0), Height(0) {}
    RectF(float x, float y, float w, float h) : X(x), Y(y), Width(w), Height(h) {}
};

struct Rect {
    int X, Y, Width, Height;
    Rect() : X(0), Y(0), Width(0), Height(0) {}
    Rect(int x, int y, int w, int h) : X(x), Y(y), Width(w), Height(h) {}
};

static inline CGColorRef cgColor(const Color& c)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGFloat v[4] = {c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0};
    CGColorRef r = CGColorCreate(cs, v);
    CGColorSpaceRelease(cs);
    return r;
}

static inline CGGradientRef makeGradient(const Color& c0, const Color& c1)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGFloat v[8] = {c0.r / 255.0, c0.g / 255.0, c0.b / 255.0, c0.a / 255.0, c1.r / 255.0, c1.g / 255.0, c1.b / 255.0, c1.a / 255.0};
    CGFloat loc[2] = {0, 1};
    CGGradientRef g = CGGradientCreateWithColorComponents(cs, v, loc, 2);
    CGColorSpaceRelease(cs);
    return g;
}

class GraphicsPath {
public:
    GraphicsPath() { path = CGPathCreateMutable(); }
    ~GraphicsPath() { CGPathRelease(path); }
    GraphicsPath(const GraphicsPath&) = delete;
    GraphicsPath& operator=(const GraphicsPath&) = delete;

    void AddArc(float x, float y, float w, float h, float start, float sweep)
    {
        CGAffineTransform t = CGAffineTransformMakeTranslation(x + w / 2, y + h / 2);
        t = CGAffineTransformScale(t, w / 2, h / 2);
        float a0 = start * (float)M_PI / 180.f, a1 = (start + sweep) * (float)M_PI / 180.f;
        CGPathAddArc(path, &t, 0, 0, 1, a0, a1, sweep < 0);
    }
    void AddEllipse(float x, float y, float w, float h) { CGPathAddEllipseInRect(path, nullptr, CGRectMake(x, y, w, h)); }
    void AddRectangle(const Rect& r)
    {
        CGPathAddRect(path, nullptr, CGRectMake(r.X, r.Y, r.Width, r.Height));
        isRect = true;
    }
    void AddPolygon(const PointF* p, int n)
    {
        if (n < 2) return;
        CGPathMoveToPoint(path, nullptr, p[0].X, p[0].Y);
        for (int i = 1; i < n; i++) CGPathAddLineToPoint(path, nullptr, p[i].X, p[i].Y);
        CGPathCloseSubpath(path);
    }
    void CloseFigure() { CGPathCloseSubpath(path); }
    CGRect bounds() const { return CGPathGetBoundingBox(path); }
    CGMutablePathRef path;
    bool isRect = false;
};

struct Region {
    int level = 0;
};

class Brush {
public:
    virtual ~Brush() {}
    virtual void fillClipped(CGContextRef c, const CGRect& area) const = 0;
    virtual bool isSolid() const { return false; }
};

class SolidBrush : public Brush {
public:
    SolidBrush(const Color& c) : col(c) {}
    void fillClipped(CGContextRef c, const CGRect& area) const override
    {
        CGContextSetRGBFillColor(c, col.r / 255.0, col.g / 255.0, col.b / 255.0, col.a / 255.0);
        CGContextFillRect(c, area);
    }
    bool isSolid() const override { return true; }
    Color col;
};

class LinearGradientBrush : public Brush {
public:
    LinearGradientBrush(const PointF& a, const PointF& b, const Color& c0, const Color& c1) : p0(a), p1(b), col0(c0), col1(c1) {}
    LinearGradientBrush(const Point& a, const Point& b, const Color& c0, const Color& c1)
        : p0((float)a.X, (float)a.Y), p1((float)b.X, (float)b.Y), col0(c0), col1(c1)
    {
    }
    void fillClipped(CGContextRef c, const CGRect&) const override
    {
        CGGradientRef g = makeGradient(col0, col1);
        CGContextDrawLinearGradient(c, g, CGPointMake(p0.X, p0.Y), CGPointMake(p1.X, p1.Y), kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
        CGGradientRelease(g);
    }
    PointF p0, p1;
    Color col0, col1;
};

class PathGradientBrush : public Brush {
public:
    PathGradientBrush(const GraphicsPath* gp)
    {
        CGRect b = gp->bounds();
        bx = (float)b.origin.x;
        by = (float)b.origin.y;
        bw = (float)b.size.width;
        bh = (float)b.size.height;
        cx = bx + bw / 2;
        cy = by + bh / 2;
        endR = gp->isRect ? 1.41421356f : 1.f;
    }
    void SetCenterPoint(const PointF& p) { cx = p.X; cy = p.Y; }
    void SetCenterColor(const Color& c) { center = c; }
    void SetSurroundColors(const Color* c, int* n)
    {
        if (c && n && *n > 0) surround = c[0];
    }
    void SetFocusScales(float x, float) { focus = x; }
    void fillClipped(CGContextRef c, const CGRect&) const override
    {
        CGGradientRef g = makeGradient(center, surround);
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, bx + bw / 2, by + bh / 2);
        CGContextScaleCTM(c, bw / 2, bh / 2);
        float sx = (cx - (bx + bw / 2)) / (bw / 2), sy = (cy - (by + bh / 2)) / (bh / 2);
        CGContextDrawRadialGradient(c, g, CGPointMake(sx, sy), focus * endR, CGPointMake(0, 0), endR, kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
        CGContextRestoreGState(c);
        CGGradientRelease(g);
    }
    float bx, by, bw, bh, cx, cy, endR;
    float focus = 0.f;
    Color center = Color(0, 0, 0, 0), surround = Color(0, 0, 0, 0);
};

class Pen {
public:
    Pen(const Color& c, float w = 1.f) : col(c), width(w) {}
    void SetStartCap(LineCap c) { cap = c; }
    void SetEndCap(LineCap c) { cap = c; }
    Color col;
    float width;
    LineCap cap = LineCapFlat;
};

class FontFamily {
public:
    FontFamily(const wchar_t*) {}
};

class Font {
public:
    Font(const FontFamily*, float size, int style, Unit) : sz(size)
    {
        CTFontRef base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
        if (style & FontStyleBold) {
            CTFontRef bold = CTFontCreateCopyWithSymbolicTraits(base, size, nullptr, kCTFontBoldTrait, kCTFontBoldTrait);
            if (bold) {
                CFRelease(base);
                base = bold;
            }
        }
        font = base;
    }
    ~Font() { if (font) CFRelease(font); }
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    CTFontRef font = nullptr;
    float sz;
};

class StringFormat {
public:
    void SetAlignment(StringAlignment a) { align = a; }
    void SetLineAlignment(StringAlignment) {}
    StringAlignment align = StringAlignmentNear;
};

static inline CFStringRef makeString(const wchar_t* s, int len)
{
    int n = len < 0 ? (int)std::wcslen(s) : len;
    std::vector<uint32_t> u((size_t)n);
    for (int i = 0; i < n; i++) u[(size_t)i] = (uint32_t)s[i];
    return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)u.data(), (CFIndex)n * 4, kCFStringEncodingUTF32LE, false);
}

static inline CTLineRef makeLine(const wchar_t* s, int len, const Font* f, CGColorRef color)
{
    CFStringRef str = makeString(s, len);
    CFStringRef keys[2] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    CFTypeRef vals[2] = {f->font, color};
    CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)vals, color ? 2 : 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(kCFAllocatorDefault, str, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(str);
    return line;
}

class Image {
public:
    Image(CGImageRef i) : img(i) {}
    ~Image() { if (img) CGImageRelease(img); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    CGImageRef img;
};

static inline Image* loadPng(const unsigned char* data, size_t size)
{
    CFDataRef d = CFDataCreate(kCFAllocatorDefault, data, (CFIndex)size);
    if (!d) return nullptr;
    CGImageSourceRef src = CGImageSourceCreateWithData(d, nullptr);
    CGImageRef im = src ? CGImageSourceCreateImageAtIndex(src, 0, nullptr) : nullptr;
    if (src) CFRelease(src);
    CFRelease(d);
    return im ? new Image(im) : nullptr;
}

class Bitmap {
public:
    Bitmap(int w_, int h_, PixelFormat) : w(w_), h(h_), scale(gDeviceScale)
    {
        int pw = (int)std::lround(w * scale), ph = (int)std::lround(h * scale);
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        ctx = CGBitmapContextCreate(nullptr, (size_t)pw, (size_t)ph, 8, 0, cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGColorSpaceRelease(cs);
        CGContextTranslateCTM(ctx, 0, ph);
        CGContextScaleCTM(ctx, scale, -scale);
    }
    ~Bitmap() { if (ctx) CGContextRelease(ctx); }
    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;
    CGContextRef ctx = nullptr;
    int w, h;
    float scale;
};

class Graphics {
public:
    Graphics(CGContextRef c) : ctx(c) { init(); }
    Graphics(Bitmap* b) : ctx(b->ctx) { init(); }
    ~Graphics() { ResetClip(); }
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    void SetSmoothingMode(SmoothingMode) {}
    void SetTextRenderingHint(TextRenderingHint) {}
    void SetInterpolationMode(InterpolationMode) { CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh); }

    void GetClip(Region* r) { r->level = depth; }
    void SetClip(const GraphicsPath* gp, CombineMode = CombineModeReplace)
    {
        CGContextSaveGState(ctx);
        depth++;
        CGContextAddPath(ctx, gp->path);
        CGContextEOClip(ctx);
    }
    void SetClip(const Region* r, CombineMode = CombineModeReplace) { restoreTo(r->level); }
    void ResetClip() { restoreTo(0); }

    void FillPath(const Brush* b, const GraphicsPath* gp) { fillPathRef(b, gp->path); }
    void DrawPath(const Pen* p, const GraphicsPath* gp) { strokePathRef(p, gp->path); }

    void FillRectangle(const Brush* b, float x, float y, float w, float h)
    {
        CGMutablePathRef p = CGPathCreateMutable();
        CGPathAddRect(p, nullptr, CGRectMake(x, y, w, h));
        fillPathRef(b, p);
        CGPathRelease(p);
    }
    void FillEllipse(const Brush* b, float x, float y, float w, float h)
    {
        CGMutablePathRef p = CGPathCreateMutable();
        CGPathAddEllipseInRect(p, nullptr, CGRectMake(x, y, w, h));
        fillPathRef(b, p);
        CGPathRelease(p);
    }
    void FillPolygon(const Brush* b, const PointF* pts, int n)
    {
        CGMutablePathRef p = polyPath(pts, n, true);
        fillPathRef(b, p);
        CGPathRelease(p);
    }
    void DrawLine(const Pen* pen, float x0, float y0, float x1, float y1)
    {
        CGMutablePathRef p = CGPathCreateMutable();
        CGPathMoveToPoint(p, nullptr, x0, y0);
        CGPathAddLineToPoint(p, nullptr, x1, y1);
        strokePathRef(pen, p);
        CGPathRelease(p);
    }
    void DrawLines(const Pen* pen, const PointF* pts, int n)
    {
        CGMutablePathRef p = polyPath(pts, n, false);
        strokePathRef(pen, p);
        CGPathRelease(p);
    }
    void DrawPolygon(const Pen* pen, const PointF* pts, int n)
    {
        CGMutablePathRef p = polyPath(pts, n, true);
        strokePathRef(pen, p);
        CGPathRelease(p);
    }
    void DrawRectangle(const Pen* pen, float x, float y, float w, float h)
    {
        CGMutablePathRef p = CGPathCreateMutable();
        CGPathAddRect(p, nullptr, CGRectMake(x, y, w, h));
        strokePathRef(pen, p);
        CGPathRelease(p);
    }
    void DrawEllipse(const Pen* pen, float x, float y, float w, float h)
    {
        CGMutablePathRef p = CGPathCreateMutable();
        CGPathAddEllipseInRect(p, nullptr, CGRectMake(x, y, w, h));
        strokePathRef(pen, p);
        CGPathRelease(p);
    }
    void DrawArc(const Pen* pen, float x, float y, float w, float h, float start, float sweep)
    {
        GraphicsPath gp;
        gp.AddArc(x, y, w, h, start, sweep);
        strokePathRef(pen, gp.path);
    }

    void DrawString(const wchar_t* s, int len, const Font* f, const PointF& at, const StringFormat* sf, const Brush* b)
    {
        const SolidBrush* sb = dynamic_cast<const SolidBrush*>(b);
        Color col = sb ? sb->col : Color(255, 0, 0, 0);
        CGColorRef cc = cgColor(col);
        CTLineRef line = makeLine(s, len, f, cc);
        CGFloat asc = 0, desc = 0, lead = 0;
        double w = CTLineGetTypographicBounds(line, &asc, &desc, &lead);
        float x = at.X;
        StringAlignment al = sf ? sf->align : StringAlignmentNear;
        if (al == StringAlignmentCenter) x -= (float)w / 2;
        else if (al == StringAlignmentFar) x -= (float)w;
        float baseline = at.Y + (float)(asc - desc) / 2;
        CGContextSaveGState(ctx);
        CGContextSetTextMatrix(ctx, CGAffineTransformMake(1, 0, 0, -1, 0, 0));
        CGContextSetTextPosition(ctx, x, baseline);
        CTLineDraw(line, ctx);
        CGContextRestoreGState(ctx);
        CFRelease(line);
        CGColorRelease(cc);
    }

    void MeasureString(const wchar_t* s, int len, const Font* f, const PointF&, RectF* out)
    {
        CTLineRef line = makeLine(s, len, f, nullptr);
        CGFloat asc = 0, desc = 0, lead = 0;
        double w = CTLineGetTypographicBounds(line, &asc, &desc, &lead);
        CFRelease(line);
        out->X = 0;
        out->Y = 0;
        out->Width = (float)w + f->sz * 0.28f;
        out->Height = (float)(asc + desc);
    }

    void DrawImage(const Bitmap* bm, int x, int y)
    {
        CGImageRef im = CGBitmapContextCreateImage(bm->ctx);
        if (!im) return;
        blit(im, (float)x, (float)y, (float)bm->w, (float)bm->h);
        CGImageRelease(im);
    }
    void DrawImage(const Image* im, const RectF& r)
    {
        if (im && im->img) blit(im->img, r.X, r.Y, r.Width, r.Height);
    }

private:
    CGContextRef ctx;
    int depth = 0;

    void init()
    {
        CGContextSetShouldAntialias(ctx, true);
        CGContextSetAllowsAntialiasing(ctx, true);
        CGContextSetShouldSmoothFonts(ctx, true);
        CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
    }
    void restoreTo(int level)
    {
        while (depth > level) {
            CGContextRestoreGState(ctx);
            depth--;
        }
    }
    static CGMutablePathRef polyPath(const PointF* p, int n, bool close)
    {
        CGMutablePathRef path = CGPathCreateMutable();
        if (n > 0) {
            CGPathMoveToPoint(path, nullptr, p[0].X, p[0].Y);
            for (int i = 1; i < n; i++) CGPathAddLineToPoint(path, nullptr, p[i].X, p[i].Y);
            if (close) CGPathCloseSubpath(path);
        }
        return path;
    }
    void fillPathRef(const Brush* b, CGPathRef p)
    {
        if (!b) return;
        CGContextSaveGState(ctx);
        CGContextAddPath(ctx, p);
        CGContextEOClip(ctx);
        CGRect area = CGPathGetBoundingBox(p);
        b->fillClipped(ctx, area);
        CGContextRestoreGState(ctx);
    }
    void strokePathRef(const Pen* pen, CGPathRef p)
    {
        CGContextSaveGState(ctx);
        CGContextAddPath(ctx, p);
        CGContextSetRGBStrokeColor(ctx, pen->col.r / 255.0, pen->col.g / 255.0, pen->col.b / 255.0, pen->col.a / 255.0);
        CGContextSetLineWidth(ctx, pen->width);
        CGContextSetLineCap(ctx, pen->cap == LineCapRound ? kCGLineCapRound : kCGLineCapButt);
        CGContextSetLineJoin(ctx, pen->cap == LineCapRound ? kCGLineJoinRound : kCGLineJoinMiter);
        CGContextStrokePath(ctx);
        CGContextRestoreGState(ctx);
    }
    void blit(CGImageRef im, float x, float y, float w, float h)
    {
        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, x, y + h);
        CGContextScaleCTM(ctx, 1, -1);
        CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), im);
        CGContextRestoreGState(ctx);
    }
};

}

namespace gfx {

static inline Gdiplus::Image* loadLogo(const unsigned char* data, unsigned long size) { return Gdiplus::loadPng(data, (size_t)size); }
static inline void startup() {}
static inline void shutdown() {}

}
