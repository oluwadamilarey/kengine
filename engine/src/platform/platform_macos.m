#include "platform.h"

// macOS platform layer using native Cocoa/AppKit
#if KPLATFORM_APPLE

#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
keys translate_keycode(unsigned short keycode);

// Custom NSWindow delegate to handle window events
@interface KohiWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) b8* quit_flagged;
@end

@implementation KohiWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    KINFO("Window close requested");
    *self.quit_flagged = TRUE;
    return YES;
}

- (void)windowDidResize:(NSNotification*)notification {
    NSWindow* window = notification.object;
    NSRect frame = [window contentRectForFrameRect:[window frame]];
    KDEBUG("Window resized: %dx%d", (i32)frame.size.width, (i32)frame.size.height);
}
@end

// Custom NSView to handle input events and Metal rendering
@interface KohiContentView : NSView
@end

@implementation KohiContentView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

// Enable layer-backed view for Metal
- (BOOL)wantsLayer {
    return YES;
}

- (CALayer*)makeBackingLayer {
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    return layer;
}

- (void)keyDown:(NSEvent*)event {
    unsigned short keycode = [event keyCode];
    keys key = translate_keycode(keycode);
    
    if (key != 0) {
        KDEBUG("Key pressed: %d (keycode: %d)", key, keycode);
        input_process_key(key, TRUE);
    }
}

- (void)keyUp:(NSEvent*)event {
    unsigned short keycode = [event keyCode];
    keys key = translate_keycode(keycode);
    
    if (key != 0) {
        KDEBUG("Key released: %d (keycode: %d)", key, keycode);
        input_process_key(key, FALSE);
    }
}

- (void)flagsChanged:(NSEvent*)event {
    // Handle modifier keys (Shift, Control, Command, Option)
    NSEventModifierFlags flags = [event modifierFlags];
    unsigned short keycode = [event keyCode];
    
    keys key = translate_keycode(keycode);
    if (key != 0) {
        // Determine if pressed or released based on flag state
        b8 pressed = FALSE;
        switch (keycode) {
            case 0x38: // Left Shift
            case 0x3C: // Right Shift
                pressed = (flags & NSEventModifierFlagShift) != 0;
                break;
            case 0x3B: // Left Control
            case 0x3E: // Right Control
                pressed = (flags & NSEventModifierFlagControl) != 0;
                break;
            case 0x3A: // Left Option
            case 0x3D: // Right Option
                pressed = (flags & NSEventModifierFlagOption) != 0;
                break;
            case 0x37: // Left Command
            case 0x36: // Right Command
                pressed = (flags & NSEventModifierFlagCommand) != 0;
                break;
        }
        
        KDEBUG("Modifier key %s: %d", pressed ? "pressed" : "released", key);
        input_process_key(key, pressed);
    }
}

- (void)mouseDown:(NSEvent*)event {
    KDEBUG("Left mouse button pressed");
    input_process_button(BUTTON_LEFT, TRUE);
}

- (void)mouseUp:(NSEvent*)event {
    KDEBUG("Left mouse button released");
    input_process_button(BUTTON_LEFT, FALSE);
}

- (void)rightMouseDown:(NSEvent*)event {
    KDEBUG("Right mouse button pressed");
    input_process_button(BUTTON_RIGHT, TRUE);
}

- (void)rightMouseUp:(NSEvent*)event {
    KDEBUG("Right mouse button released");
    input_process_button(BUTTON_RIGHT, FALSE);
}

- (void)otherMouseDown:(NSEvent*)event {
    if ([event buttonNumber] == 2) {
        KDEBUG("Middle mouse button pressed");
        input_process_button(BUTTON_MIDDLE, TRUE);
    }
}

- (void)otherMouseUp:(NSEvent*)event {
    if ([event buttonNumber] == 2) {
        KDEBUG("Middle mouse button released");
        input_process_button(BUTTON_MIDDLE, FALSE);
    }
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint location = [event locationInWindow];
    // Flip Y coordinate (Cocoa origin is bottom-left, we want top-left)
    NSRect frame = [self bounds];
    i32 x = (i32)location.x;
    i32 y = (i32)(frame.size.height - location.y);
    
    // Only log every ~30 frames to avoid spam
    static i32 mouse_log_counter = 0;
    if (++mouse_log_counter >= 30) {
        KDEBUG("Mouse moved: %d, %d", x, y);
        mouse_log_counter = 0;
    }
    
    input_process_mouse_move(x, y);
}

- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)scrollWheel:(NSEvent*)event {
    f64 delta_x = [event scrollingDeltaX];
    f64 delta_y = [event scrollingDeltaY];
    KDEBUG("Mouse scroll: %f, %f", delta_x, delta_y);
    // TODO: Call input_process_mouse_wheel if you have it
}

@end

typedef struct internal_state {
    NSWindow* window;
    KohiContentView* content_view;
    KohiWindowDelegate* window_delegate;
    CAMetalLayer* metal_layer;
    
    b8 quit_flagged;
    
    // Timing
    mach_timebase_info_data_t timebase_info;
    u64 start_time;
} internal_state;

b8 platform_startup(
    platform_state* plat_state,
    const char* application_name,
    i32 x,
    i32 y,
    i32 width,
    i32 height) {
    
    @autoreleasepool {
        // Create internal state
        plat_state->internal_state = malloc(sizeof(internal_state));
        internal_state* state = (internal_state*)plat_state->internal_state;
        memset(state, 0, sizeof(internal_state));
        
        state->quit_flagged = FALSE;
        
        // Initialize NSApplication if not already initialized
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // Create menu bar for proper macOS app behavior
        NSMenu* menubar = [[NSMenu alloc] init];
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [NSApp setMainMenu:menubar];
        
        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* appName = [NSString stringWithUTF8String:application_name];
        NSString* quitTitle = [@"Quit " stringByAppendingString:appName];
        NSMenuItem* quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                              action:@selector(terminate:)
                                                       keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];
        
        // Create window style mask
        NSWindowStyleMask style_mask = NSWindowStyleMaskTitled |
                                       NSWindowStyleMaskClosable |
                                       NSWindowStyleMaskMiniaturizable |
                                       NSWindowStyleMaskResizable;
        
        // Create window rect
        NSRect window_rect = NSMakeRect(x, y, width, height);
        
        // Create window
        state->window = [[NSWindow alloc] initWithContentRect:window_rect
                                                     styleMask:style_mask
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
        
        if (!state->window) {
            KFATAL("Failed to create NSWindow");
            return FALSE;
        }
        
        // Set window title
        [state->window setTitle:appName];
        
        // Create and set window delegate
        state->window_delegate = [[KohiWindowDelegate alloc] init];
        state->window_delegate.quit_flagged = &state->quit_flagged;
        [state->window setDelegate:state->window_delegate];
        
        // Create content view
        state->content_view = [[KohiContentView alloc] initWithFrame:window_rect];
        [state->window setContentView:state->content_view];
        
        // Get Metal layer for rendering
        state->metal_layer = (CAMetalLayer*)[state->content_view layer];
        
        // Configure window
        [state->window makeKeyAndOrderFront:nil];
        [state->window setAcceptsMouseMovedEvents:YES];
        [state->window makeFirstResponder:state->content_view];
        [NSApp activateIgnoringOtherApps:YES];
        
        // Setup high-precision timing
        mach_timebase_info(&state->timebase_info);
        state->start_time = mach_absolute_time();
        
        KINFO("macOS platform initialized: %dx%d at (%d, %d)", width, height, x, y);
        return TRUE;
    }
}

void platform_shutdown(platform_state* plat_state) {
    @autoreleasepool {
        internal_state* state = (internal_state*)plat_state->internal_state;
        
        if (state->window) {
            [state->window setDelegate:nil];
            [state->window close];
            state->window = nil;
        }
        
        state->window_delegate = nil;
        state->content_view = nil;
        
        free(plat_state->internal_state);
        plat_state->internal_state = NULL;
        
        KINFO("macOS platform shutdown");
    }
}

b8 platform_pump_messages(platform_state* plat_state) {
    @autoreleasepool {
        internal_state* state = (internal_state*)plat_state->internal_state;
        
        // Process all pending events
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:nil
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]) != nil) {
            [NSApp sendEvent:event];
            [NSApp updateWindows];
        }
        
        return !state->quit_flagged;
    }
}

void* platform_allocate(u64 size, b8 aligned) {
    if (aligned) {
        void* ptr;
        if (posix_memalign(&ptr, 16, size) == 0) {
            return ptr;
        }
        return NULL;
    }
    return malloc(size);
}

void platform_free(void* block, b8 aligned) {
    if (block) {
        free(block);
    }
}

void* platform_zero_memory(void* block, u64 size) {
    return memset(block, 0, size);
}

void* platform_copy_memory(void* dest, const void* source, u64 size) {
    return memcpy(dest, source, size);
}

void* platform_set_memory(void* dest, i32 value, u64 size) {
    return memset(dest, value, size);
}

void platform_console_write(const char* message, u8 colour) {
    const char* colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};
    printf("\033[%sm%s\033[0m", colour_strings[colour], message);
    fflush(stdout);
}

void platform_console_write_error(const char* message, u8 colour) {
    const char* colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};
    fprintf(stderr, "\033[%sm%s\033[0m", colour_strings[colour], message);
    fflush(stderr);
}

f64 platform_get_absolute_time(platform_state* plat_state) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    
    u64 now = mach_absolute_time();
    u64 elapsed = now - state->start_time;
    
    return (f64)elapsed * state->timebase_info.numer / 
           (state->timebase_info.denom * 1000000000.0);
}

void platform_sleep(u64 ms) {
    if (ms == 0) return;
    
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&req, NULL);
}

// Key translation from macOS keycodes
keys translate_keycode(unsigned short keycode) {
    switch (keycode) {
        case 0x00: return KEY_A;
        case 0x01: return KEY_S;
        case 0x02: return KEY_D;
        case 0x03: return KEY_F;
        case 0x04: return KEY_H;
        case 0x05: return KEY_G;
        case 0x06: return KEY_Z;
        case 0x07: return KEY_X;
        case 0x08: return KEY_C;
        case 0x09: return KEY_V;
        case 0x0B: return KEY_B;
        case 0x0C: return KEY_Q;
        case 0x0D: return KEY_W;
        case 0x0E: return KEY_E;
        case 0x0F: return KEY_R;
        case 0x10: return KEY_Y;
        case 0x11: return KEY_T;
        case 0x12: return KEY_1;
        case 0x13: return KEY_2;
        case 0x14: return KEY_3;
        case 0x15: return KEY_4;
        case 0x16: return KEY_6;
        case 0x17: return KEY_5;
        case 0x18: return KEY_EQUAL;
        case 0x19: return KEY_9;
        case 0x1A: return KEY_7;
        case 0x1B: return KEY_MINUS;
        case 0x1C: return KEY_8;
        case 0x1D: return KEY_0;
        case 0x1E: return KEY_RBRACKET;
        case 0x1F: return KEY_O;
        case 0x20: return KEY_U;
        case 0x21: return KEY_LBRACKET;
        case 0x22: return KEY_I;
        case 0x23: return KEY_P;
        case 0x24: return KEY_ENTER;
        case 0x25: return KEY_L;
        case 0x26: return KEY_J;
        case 0x27: return KEY_APOSTROPHE;
        case 0x28: return KEY_K;
        case 0x29: return KEY_SEMICOLON;
        case 0x2A: return KEY_BACKSLASH;
        case 0x2B: return KEY_COMMA;
        case 0x2C: return KEY_SLASH;
        case 0x2D: return KEY_N;
        case 0x2E: return KEY_M;
        case 0x2F: return KEY_PERIOD;
        case 0x30: return KEY_TAB;
        case 0x31: return KEY_SPACE;
        case 0x32: return KEY_GRAVE;
        case 0x33: return KEY_BACKSPACE;
        case 0x35: return KEY_ESCAPE;
        case 0x37: return KEY_LWIN;
        case 0x38: return KEY_LSHIFT;
        case 0x39: return KEY_CAPITAL;
        case 0x3A: return KEY_LMENU;
        case 0x3B: return KEY_LCONTROL;
        case 0x3C: return KEY_RSHIFT;
        case 0x3D: return KEY_RMENU;
        case 0x3E: return KEY_RCONTROL;
        
        // Function keys
        case 0x7A: return KEY_F1;
        case 0x78: return KEY_F2;
        case 0x63: return KEY_F3;
        case 0x76: return KEY_F4;
        case 0x60: return KEY_F5;
        case 0x61: return KEY_F6;
        case 0x62: return KEY_F7;
        case 0x64: return KEY_F8;
        case 0x65: return KEY_F9;
        case 0x6D: return KEY_F10;
        case 0x67: return KEY_F11;
        case 0x6F: return KEY_F12;
        
        // Arrow keys
        case 0x7B: return KEY_LEFT;
        case 0x7C: return KEY_RIGHT;
        case 0x7D: return KEY_DOWN;
        case 0x7E: return KEY_UP;
        
        // Numpad
        case 0x52: return KEY_NUMPAD0;
        case 0x53: return KEY_NUMPAD1;
        case 0x54: return KEY_NUMPAD2;
        case 0x55: return KEY_NUMPAD3;
        case 0x56: return KEY_NUMPAD4;
        case 0x57: return KEY_NUMPAD5;
        case 0x58: return KEY_NUMPAD6;
        case 0x59: return KEY_NUMPAD7;
        case 0x5B: return KEY_NUMPAD8;
        case 0x5C: return KEY_NUMPAD9;
        case 0x45: return KEY_ADD;
        case 0x4E: return KEY_SUBTRACT;
        case 0x43: return KEY_MULTIPLY;
        case 0x4B: return KEY_DIVIDE;
        case 0x41: return KEY_DECIMAL;
        
        // Other
        case 0x73: return KEY_HOME;
        case 0x77: return KEY_END;
        case 0x74: return KEY_PRIOR;
        case 0x79: return KEY_NEXT;
        case 0x75: return KEY_DELETE;
        case 0x72: return KEY_INSERT;
        
        default: return 0;
    }
}

#endif // KPLATFORM_APPLE