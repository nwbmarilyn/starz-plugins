#import <Cocoa/Cocoa.h>
#include "ui.h"

#ifndef STARZ_NSVIEW_CLASS
#define STARZ_NSVIEW_CLASS StarzNSView
#endif

@interface STARZ_NSVIEW_CLASS : NSView {
@public
    ui::BaseView* core;
}
@end

@implementation STARZ_NSVIEW_CLASS

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)e { return YES; }
- (BOOL)isOpaque { return YES; }

- (void)updateTrackingAreas
{
    for (NSTrackingArea* a in [self trackingAreas]) [self removeTrackingArea:a];
    NSTrackingAreaOptions o = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways | NSTrackingInVisibleRect;
    NSTrackingArea* t = [[NSTrackingArea alloc] initWithRect:[self bounds] options:o owner:self userInfo:nil];
    [self addTrackingArea:t];
    [t release];
    [super updateTrackingAreas];
}

- (void)drawRect:(NSRect)dirty
{
    if (!core) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    float s = self.window ? (float)self.window.backingScaleFactor : 1.f;
    if (s < 1.f) s = 1.f;
    Gdiplus::gDeviceScale = s;
    CGContextSaveGState(ctx);
    Gdiplus::Graphics g(ctx);
    core->paintAll(g);
    CGContextRestoreGState(ctx);
}

- (void)tick:(NSTimer*)t
{
    if (!core) return;
    core->tick();
    [self setNeedsDisplay:YES];
}

- (NSPoint)local:(NSEvent*)e { return [self convertPoint:[e locationInWindow] fromView:nil]; }

- (void)mouseDown:(NSEvent*)e
{
    if (!core) return;
    NSPoint p = [self local:e];
    if ([e modifierFlags] & NSEventModifierFlagControl) {
        core->rightDown((int)p.x, (int)p.y);
    } else {
        core->mouseDown((int)p.x, (int)p.y, ([e modifierFlags] & NSEventModifierFlagShift) != 0);
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent*)e
{
    if (!core) return;
    NSPoint p = [self local:e];
    core->mouseMove((int)p.x, (int)p.y, ([e modifierFlags] & NSEventModifierFlagShift) != 0);
    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent*)e
{
    if (!core) return;
    NSPoint p = [self local:e];
    core->mouseMove((int)p.x, (int)p.y, false);
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)e
{
    if (!core) return;
    core->mouseLeave();
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent*)e
{
    if (!core) return;
    core->mouseUp();
    [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent*)e
{
    if (!core) return;
    NSPoint p = [self local:e];
    core->rightDown((int)p.x, (int)p.y);
    [self setNeedsDisplay:YES];
}

- (void)scrollWheel:(NSEvent*)e
{
    if (!core) return;
    NSPoint p = [self local:e];
    CGFloat d = [e scrollingDeltaY];
    if (d == 0) return;
    core->wheel((int)p.x, (int)p.y, d > 0 ? 1 : -1);
    [self setNeedsDisplay:YES];
}

@end

namespace ui {

BaseView::~BaseView()
{
    if (nsView) removed();
}

Steinberg::tresult PLUGIN_API BaseView::isPlatformTypeSupported(Steinberg::FIDString type)
{
    return type && strcmp(type, Steinberg::kPlatformTypeNSView) == 0 ? Steinberg::kResultTrue : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API BaseView::attached(void* parent, Steinberg::FIDString type)
{
    if (!parent || nsView || isPlatformTypeSupported(type) != Steinberg::kResultTrue) return Steinberg::kResultFalse;
    acquire();
    init();
    STARZ_NSVIEW_CLASS* v = [[STARZ_NSVIEW_CLASS alloc] initWithFrame:NSMakeRect(0, 0, kW, kH)];
    v->core = this;
    [(NSView*)parent addSubview:v];
    NSTimer* t = [NSTimer timerWithTimeInterval:0.033 target:v selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:t forMode:NSRunLoopCommonModes];
    nsView = v;
    nsTimer = t;
    return Steinberg::CPluginView::attached(parent, type);
}

Steinberg::tresult PLUGIN_API BaseView::removed()
{
    if (nsView) {
        STARZ_NSVIEW_CLASS* v = (STARZ_NSVIEW_CLASS*)nsView;
        [(NSTimer*)nsTimer invalidate];
        v->core = nullptr;
        [v removeFromSuperview];
        [v release];
        nsView = nullptr;
        nsTimer = nullptr;
        dropCache();
        ui::release();
    }
    return Steinberg::CPluginView::removed();
}

Steinberg::tresult PLUGIN_API BaseView::onSize(Steinberg::ViewRect* r)
{
    if (r) rect = *r;
    return Steinberg::kResultTrue;
}

bool BaseView::pickFile(bool save, userpresets::fs::path& out)
{
    userpresets::ensureDir(userpresets::dirFor(C.sub));
    NSString* dir = [NSString stringWithUTF8String:userpresets::dirFor(C.sub).string().c_str()];
    NSURL* dirUrl = [NSURL fileURLWithPath:dir isDirectory:YES];
    NSArray* types = @[ @"starz" ];
    NSURL* chosen = nil;
    if (save) {
        NSSavePanel* p = [NSSavePanel savePanel];
        p.directoryURL = dirUrl;
        p.nameFieldStringValue = @"My preset.starz";
        p.allowedFileTypes = types;
        p.canCreateDirectories = YES;
        if ([p runModal] == NSModalResponseOK) chosen = p.URL;
    } else {
        NSOpenPanel* p = [NSOpenPanel openPanel];
        p.directoryURL = dirUrl;
        p.allowedFileTypes = types;
        p.canChooseFiles = YES;
        p.canChooseDirectories = NO;
        p.allowsMultipleSelection = NO;
        if ([p runModal] == NSModalResponseOK) chosen = p.URL;
    }
    if (!chosen) return false;
    out = userpresets::fs::path([[chosen path] UTF8String]);
    if (save && out.extension() != ".starz") out += ".starz";
    return true;
}

}
