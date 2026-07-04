/**
 * platform_macos.m
 * Kohi Engine – macOS platform layer (Cocoa + CAMetalLayer + MoltenVK)
 */

#include "platform.h"

#ifdef KPLATFORM_APPLE

#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"
#include "containers/darray.h"
#include "renderer/vulkan/vulkan_types.inl"

/*
 * MoltenVK surface extension.
 *
 * DO NOT rely on VK_USE_PLATFORM_METAL_EXT + <vulkan/vulkan.h> alone.
 * That gate only works if the Vulkan loader's vulkan.h knows to include
 * vulkan_metal.h, which requires the MoltenVK SDK headers on the include
 * path with a sufficiently recent version.  The robust approach is to
 * include vulkan_metal.h directly after the core header.
 *
 * Precedence note: ensure $(VULKAN_SDK)/include appears in INCLUDE_FLAGS
 * BEFORE any system include path (e.g. /usr/local/include) so the MoltenVK
 * headers win over a bare Vulkan loader install that lacks vulkan_metal.h.
 */
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>   /* VkMetalSurfaceCreateInfoEXT, vkCreateMetalSurfaceEXT */

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

keys translate_keycode(unsigned short keycode);

/* -------------------------------------------------------------------------
 * Window delegate
 * ---------------------------------------------------------------------- */
@interface KohiWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) b8 *quit_flagged;
@end

@implementation KohiWindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender {
    KINFO("Window close requested");
    *self.quit_flagged = true;
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification {
    NSWindow *window = notification.object;
    NSRect frame = [window contentRectForFrameRect:[window frame]];
    event_context ctx;
    ctx.data.u16[0] = (u16)frame.size.width;
    ctx.data.u16[1] = (u16)frame.size.height;
    event_fire(EVENT_CODE_RESIZED, 0, ctx);
}

@end

/* -------------------------------------------------------------------------
 * Content view
 * ---------------------------------------------------------------------- */
@interface KohiContentView : NSView
@property (nonatomic, strong) CAMetalLayer *metalLayer;
@end

@implementation KohiContentView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (!self) return nil;

    [self setWantsLayer:YES];

    self.metalLayer                 = [CAMetalLayer layer];
    self.metalLayer.pixelFormat     = MTLPixelFormatBGRA8Unorm;
    self.metalLayer.framebufferOnly = YES;
    self.metalLayer.frame           = self.bounds;
    self.layerContentsRedrawPolicy  = NSViewLayerContentsRedrawDuringViewResize;

    [self setLayer:self.metalLayer];
    KDEBUG("KohiContentView: CAMetalLayer %p created", self.metalLayer);
    return self;
}

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)canBecomeKeyView      { return YES; }
- (BOOL)wantsUpdateLayer      { return YES; }

- (void)updateLayer {
    self.metalLayer.frame = self.bounds;
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    NSScreen *screen = self.window.screen ?: [NSScreen mainScreen];
    CGFloat scale = screen.backingScaleFactor;
    if (self.metalLayer) {
        self.metalLayer.contentsScale = scale;
        self.metalLayer.drawableSize  = CGSizeMake(
            self.bounds.size.width  * scale,
            self.bounds.size.height * scale);
    }
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    if (self.metalLayer) {
        self.metalLayer.frame = self.bounds;
        CGFloat scale = self.metalLayer.contentsScale;
        self.metalLayer.drawableSize = CGSizeMake(
            self.bounds.size.width  * scale,
            self.bounds.size.height * scale);
    }
}

/* Keyboard */
- (void)keyDown:(NSEvent *)event {
    keys key = translate_keycode([event keyCode]);
    if (key) input_process_key(key, true);
}
- (void)keyUp:(NSEvent *)event {
    keys key = translate_keycode([event keyCode]);
    if (key) input_process_key(key, false);
}
- (void)flagsChanged:(NSEvent *)event {
    NSEventModifierFlags flags = [event modifierFlags];
    unsigned short kc = [event keyCode];
    keys key = translate_keycode(kc);
    if (!key) return;
    b8 pressed = false;
    switch (kc) {
        case 0x38: case 0x3C: pressed = (flags & NSEventModifierFlagShift)   != 0; break;
        case 0x3B: case 0x3E: pressed = (flags & NSEventModifierFlagControl) != 0; break;
        case 0x3A: case 0x3D: pressed = (flags & NSEventModifierFlagOption)  != 0; break;
        case 0x37: case 0x36: pressed = (flags & NSEventModifierFlagCommand) != 0; break;
    }
    input_process_key(key, pressed);
}

/* Mouse buttons */
- (void)mouseDown:(NSEvent *)e      { input_process_button(BUTTON_LEFT,   true);  }
- (void)mouseUp:(NSEvent *)e        { input_process_button(BUTTON_LEFT,   false); }
- (void)rightMouseDown:(NSEvent *)e { input_process_button(BUTTON_RIGHT,  true);  }
- (void)rightMouseUp:(NSEvent *)e   { input_process_button(BUTTON_RIGHT,  false); }
- (void)otherMouseDown:(NSEvent *)e {
    if ([e buttonNumber] == 2) input_process_button(BUTTON_MIDDLE, true);
}
- (void)otherMouseUp:(NSEvent *)e {
    if ([e buttonNumber] == 2) input_process_button(BUTTON_MIDDLE, false);
}

/* Mouse movement */
- (void)_processMoveEvent:(NSEvent *)event {
    NSPoint loc    = [event locationInWindow];
    NSRect  bounds = [self bounds];
    i32 x = (i32)loc.x;
    i32 y = (i32)(bounds.size.height - loc.y); /* flip Y: AppKit=BL, Kohi=TL */
    input_process_mouse_move(x, y);
}
- (void)mouseMoved:(NSEvent *)e        { [self _processMoveEvent:e]; }
- (void)mouseDragged:(NSEvent *)e      { [self _processMoveEvent:e]; }
- (void)rightMouseDragged:(NSEvent *)e { [self _processMoveEvent:e]; }
- (void)otherMouseDragged:(NSEvent *)e { [self _processMoveEvent:e]; }

/* Scroll */
- (void)scrollWheel:(NSEvent *)event {
    f64 delta = [event scrollingDeltaY];
    if ([event hasPreciseScrollingDeltas]) delta *= 0.1;
    if (delta != 0) input_process_mouse_wheel(delta > 0 ? 1 : -1);
}

@end

/* -------------------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------------- */
typedef struct internal_state {
    NSWindow           *window;
    KohiContentView    *content_view;
    KohiWindowDelegate *window_delegate;
    CAMetalLayer       *metal_layer;
    b8                  quit_flagged;
    mach_timebase_info_data_t timebase_info;
    u64                       start_time;
} internal_state;



/* -------------------------------------------------------------------------
 * platform_startup
 * ---------------------------------------------------------------------- */
b8 platform_startup(
    platform_state *plat_state,
    const char     *application_name,
    i32 x, i32 y, i32 width, i32 height)
{
    @autoreleasepool {
        plat_state->internal_state = malloc(sizeof(internal_state));
        internal_state *state = (internal_state *)plat_state->internal_state;
        memset(state, 0, sizeof(internal_state));

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSMenu     *menubar  = [[NSMenu alloc] init];
        NSMenuItem *appItem  = [[NSMenuItem alloc] init];
        [menubar addItem:appItem];
        [NSApp setMainMenu:menubar];

        NSMenu     *appMenu   = [[NSMenu alloc] init];
        NSString   *appName   = [NSString stringWithUTF8String:application_name];
        NSString   *quitTitle = [@"Quit " stringByAppendingString:appName];
        NSMenuItem *quitItem  = [[NSMenuItem alloc]
                                     initWithTitle:quitTitle
                                            action:@selector(terminate:)
                                     keyEquivalent:@"q"];
        [appMenu addItem:quitItem];
        [appItem setSubmenu:appMenu];

        NSWindowStyleMask style =
            NSWindowStyleMaskTitled         |
            NSWindowStyleMaskClosable       |
            NSWindowStyleMaskMiniaturizable |
            NSWindowStyleMaskResizable;

        state->window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(x, y, width, height)
                      styleMask:style
                        backing:NSBackingStoreBuffered
                          defer:NO];
        if (!state->window) {
            KFATAL("Failed to create NSWindow");
            return false;
        }
        [state->window setTitle:appName];

        state->window_delegate              = [[KohiWindowDelegate alloc] init];
        state->window_delegate.quit_flagged = &state->quit_flagged;
        [state->window setDelegate:state->window_delegate];

        state->content_view = [[KohiContentView alloc]
            initWithFrame:NSMakeRect(0, 0, width, height)];
        [state->window setContentView:state->content_view];

        state->metal_layer = state->content_view.metalLayer;
        if (!state->metal_layer) {
            KFATAL("Failed to obtain CAMetalLayer");
            return false;
        }

        CGFloat scale = [[NSScreen mainScreen] backingScaleFactor];
        state->metal_layer.contentsScale = scale;
        state->metal_layer.drawableSize  = CGSizeMake(width * scale, height * scale);

        [state->window makeKeyAndOrderFront:nil];
        [state->window setAcceptsMouseMovedEvents:YES];
        [state->window makeFirstResponder:state->content_view];
        [NSApp activateIgnoringOtherApps:YES];

        mach_timebase_info(&state->timebase_info);
        state->start_time = mach_absolute_time();

        KINFO("macOS platform started: %dx%d @ (%d,%d) Retina×%.1f",
              width, height, x, y, scale);
        return true;
    }
}

/* -------------------------------------------------------------------------
 * platform_shutdown
 * ---------------------------------------------------------------------- */
void platform_shutdown(platform_state *plat_state) {
    @autoreleasepool {
        internal_state *state = (internal_state *)plat_state->internal_state;
        if (state->window) {
            [state->window setDelegate:nil];
            [state->window close];
            state->window = nil;
        }
        state->window_delegate = nil;
        state->content_view    = nil;
        free(plat_state->internal_state);
        plat_state->internal_state = NULL;
        KINFO("macOS platform shutdown");
    }
}

/* -------------------------------------------------------------------------
 * platform_pump_messages
 * MUST use [NSDate distantPast] – NOT nil (nil blocks until next event).
 * ---------------------------------------------------------------------- */
b8 platform_pump_messages(platform_state *plat_state) {
    @autoreleasepool {
        internal_state *state = (internal_state *)plat_state->internal_state;

        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]) != nil) {
            [NSApp sendEvent:event];
            [NSApp updateWindows];
        }

        return !state->quit_flagged;
    }
}

/* -------------------------------------------------------------------------
 * Memory
 * ---------------------------------------------------------------------- */
void *platform_allocate(u64 size, b8 aligned) {
    if (aligned) {
        void *ptr = NULL;
        return posix_memalign(&ptr, 16, size) == 0 ? ptr : NULL;
    }
    return malloc(size);
}

void  platform_free(void *block, b8 aligned)                      { free(block); }
void *platform_zero_memory(void *block, u64 size)                 { return memset(block, 0, size); }
void *platform_copy_memory(void *dest, const void *src, u64 size) { return memcpy(dest, src, size); }
void *platform_set_memory(void *dest, i32 value, u64 size)        { return memset(dest, value, size); }

/* -------------------------------------------------------------------------
 * Console
 * ---------------------------------------------------------------------- */
void platform_console_write(const char *message, u8 colour) {
    static const char *codes[] = {"0;41","1;31","1;33","1;32","1;34","1;30"};
    printf("\033[%sm%s\033[0m", codes[colour], message);
    fflush(stdout);
}
void platform_console_write_error(const char *message, u8 colour) {
    static const char *codes[] = {"0;41","1;31","1;33","1;32","1;34","1;30"};
    fprintf(stderr, "\033[%sm%s\033[0m", codes[colour], message);
    fflush(stderr);
}

/* -------------------------------------------------------------------------
 * Clock  –  signature matches platform.h exactly
 * ---------------------------------------------------------------------- */
f64 platform_get_absolute_time(platform_state *plat_state) {
    internal_state *state = (internal_state *)plat_state->internal_state;
    u64 elapsed = mach_absolute_time() - state->start_time;
    return (f64)elapsed
         * (f64)state->timebase_info.numer
         / ((f64)state->timebase_info.denom * 1.0e9);
}

void platform_sleep(u64 ms) {
    struct timespec req = {
        .tv_sec  = (time_t)(ms / 1000),
        .tv_nsec = (long)((ms % 1000) * 1000000)
    };
    nanosleep(&req, NULL);
}

/* -------------------------------------------------------------------------
 * Vulkan extensions
 *
 * darray element type is `const char *` — take address of a local pointer,
 * not &"literal" or &MACRO (those give you a const char(*)[N], wrong size).
 * ---------------------------------------------------------------------- */
void platform_get_required_extension_names(const char ***names_darray) {
    /*
     * VK_KHR_portability_enumeration  (required since SDK 1.3.216)
     * Without it vkCreateInstance returns VK_ERROR_INCOMPATIBLE_DRIVER.
     * Must be paired with VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
     * in VkInstanceCreateInfo.flags inside vulkan_backend.c.
     */
    const char *portability   = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    darray_push(*names_darray, portability);

    /*
     * VK_EXT_metal_surface – WSI extension that exposes
     * vkCreateMetalSurfaceEXT, which turns a CAMetalLayer into a VkSurfaceKHR.
     */
    const char *metal_surface = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
    darray_push(*names_darray, metal_surface);
}

/* -------------------------------------------------------------------------
 * Vulkan surface
 * ---------------------------------------------------------------------- */
b8 platform_create_vulkan_surface(
    platform_state *plat_state,
    vulkan_context *context)
{
    @autoreleasepool {
        internal_state *state = (internal_state *)plat_state->internal_state;

        if (!state || !state->metal_layer) {
            KERROR("platform_create_vulkan_surface: metal_layer is NULL");
            return false;
        }

        /* C99 zero-initialiser  (= {} is C++, not C99) */
        VkMetalSurfaceCreateInfoEXT create_info = {0};
        create_info.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        create_info.pLayer = (__bridge const CAMetalLayer *)state->metal_layer;

        VkResult result = vkCreateMetalSurfaceEXT(
            context->instance,
            &create_info,
            context->allocator,
            &context->surface);

        if (result != VK_SUCCESS) {
            KERROR("vkCreateMetalSurfaceEXT failed: %d", result);
            return false;
        }

        KINFO("Vulkan Metal surface created (handle=%p)", (void *)context->surface);
        return true;
    }
}

/* -------------------------------------------------------------------------
 * Key translation
 * ---------------------------------------------------------------------- */
keys translate_keycode(unsigned short kc) {
    switch (kc) {
        case 0x00: return KEY_A;          case 0x01: return KEY_S;
        case 0x02: return KEY_D;          case 0x03: return KEY_F;
        case 0x04: return KEY_H;          case 0x05: return KEY_G;
        case 0x06: return KEY_Z;          case 0x07: return KEY_X;
        case 0x08: return KEY_C;          case 0x09: return KEY_V;
        case 0x0B: return KEY_B;          case 0x0C: return KEY_Q;
        case 0x0D: return KEY_W;          case 0x0E: return KEY_E;
        case 0x0F: return KEY_R;          case 0x10: return KEY_Y;
        case 0x11: return KEY_T;          case 0x12: return KEY_1;
        case 0x13: return KEY_2;          case 0x14: return KEY_3;
        case 0x15: return KEY_4;          case 0x16: return KEY_6;
        case 0x17: return KEY_5;          case 0x18: return KEY_EQUAL;
        case 0x19: return KEY_9;          case 0x1A: return KEY_7;
        case 0x1B: return KEY_MINUS;      case 0x1C: return KEY_8;
        case 0x1D: return KEY_0;          case 0x1E: return KEY_RBRACKET;
        case 0x1F: return KEY_O;          case 0x20: return KEY_U;
        case 0x21: return KEY_LBRACKET;   case 0x22: return KEY_I;
        case 0x23: return KEY_P;          case 0x24: return KEY_ENTER;
        case 0x25: return KEY_L;          case 0x26: return KEY_J;
        case 0x27: return KEY_APOSTROPHE; case 0x28: return KEY_K;
        case 0x29: return KEY_SEMICOLON;  case 0x2A: return KEY_BACKSLASH;
        case 0x2B: return KEY_COMMA;      case 0x2C: return KEY_SLASH;
        case 0x2D: return KEY_N;          case 0x2E: return KEY_M;
        case 0x2F: return KEY_PERIOD;     case 0x30: return KEY_TAB;
        case 0x31: return KEY_SPACE;      case 0x32: return KEY_GRAVE;
        case 0x33: return KEY_BACKSPACE;  case 0x35: return KEY_ESCAPE;
        case 0x36: return KEY_RWIN;       case 0x37: return KEY_LWIN;
        case 0x38: return KEY_LSHIFT;     case 0x39: return KEY_CAPITAL;
        //case 0x3A: return KEY_LMENU;      
        case 0x3B: return KEY_LCONTROL;
        case 0x3C: return KEY_RSHIFT;     //case 0x3D: return KEY_RMENU;
        case 0x3E: return KEY_RCONTROL;

        case 0x7A: return KEY_F1;         case 0x78: return KEY_F2;
        case 0x63: return KEY_F3;         case 0x76: return KEY_F4;
        case 0x60: return KEY_F5;         case 0x61: return KEY_F6;
        case 0x62: return KEY_F7;         case 0x64: return KEY_F8;
        case 0x65: return KEY_F9;         case 0x6D: return KEY_F10;
        case 0x67: return KEY_F11;        case 0x6F: return KEY_F12;

        case 0x7B: return KEY_LEFT;       case 0x7C: return KEY_RIGHT;
        case 0x7D: return KEY_DOWN;       case 0x7E: return KEY_UP;

        case 0x52: return KEY_NUMPAD0;    case 0x53: return KEY_NUMPAD1;
        case 0x54: return KEY_NUMPAD2;    case 0x55: return KEY_NUMPAD3;
        case 0x56: return KEY_NUMPAD4;    case 0x57: return KEY_NUMPAD5;
        case 0x58: return KEY_NUMPAD6;    case 0x59: return KEY_NUMPAD7;
        case 0x5B: return KEY_NUMPAD8;    case 0x5C: return KEY_NUMPAD9;
        case 0x45: return KEY_ADD;        case 0x4E: return KEY_SUBTRACT;
        case 0x43: return KEY_MULTIPLY;   case 0x4B: return KEY_DIVIDE;
        case 0x41: return KEY_DECIMAL;

        case 0x73: return KEY_HOME;       case 0x77: return KEY_END;
        case 0x74: return KEY_PRIOR;      case 0x79: return KEY_NEXT;
        case 0x75: return KEY_DELETE;     case 0x72: return KEY_INSERT;

        case 0x3A: return KEY_LALT;
        case 0x3D: return KEY_RALT;

        default: return 0;
    }
}

#endif /* KPLATFORM_APPLE */