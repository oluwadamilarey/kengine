# Platform Layer (`engine/src/platform`)

This directory is the only place in the engine that is allowed to talk to the operating
system directly. Everything above it — the application loop, the clock, input, the
renderer — reaches the OS exclusively through the interface declared in
[`platform.h`](platform.h).

This document explains what actually happens at runtime on each platform, with
particular detail on the two pieces the rest of the engine depends on most:
**the clock (timing)** and **the window system (macOS/Cocoa)**.

---

## Contents

- [The abstraction](#the-abstraction)
- [File map](#file-map)
- [Lifecycle & ownership](#lifecycle--ownership)
- [The clock & timing](#the-clock--timing)
- [The window system (macOS)](#the-window-system-macos)
- [Event pumping & input](#event-pumping--input)
- [Other platform services](#other-platform-services)
- [Known quirks & pitfalls](#known-quirks--pitfalls)

---

## The abstraction

`platform.h` defines the contract every OS implementation must fulfil:

```c
typedef struct platform_state {
    void* internal_state;   // opaque per-OS state (e.g. NSWindow, HWND, X11 data)
} platform_state;
```

Key points:

- **`platform_state` is opaque.** It holds a single `void*` that each backend
  fills with its own internal struct. Callers outside this directory never look
  inside it.
- **State flows down, not up.** The application owns one `platform_state` in
  `application_state.platform` and passes pointers to it into every platform
  call. The platform layer never allocates a global app state.
- The contract covers: window create/teardown, message pumping, memory
  primitives, console output, absolute time, sleep, the Vulkan surface creation,
  and the device pixel ratio.

## File map

| File | Role |
|---|---|
| `platform.h` | Cross-platform interface + `platform_state` |
| `platform_macos.m` | Full implementation: Cocoa window, CAMetalLayer, Mach timers, MoltenVK surface |
| `platform_win32.c` | Windows implementation (Win32 window, `QueryPerformanceCounter`) |
| `platform_linux.c` | Linux implementation (X11, `clock_gettime(CLOCK_MONOTONIC)`) |
| `filesystem.*` | File read/write used by asset loading and the log file |

---

## Lifecycle & ownership

`application.c` (`application_create`) initializes subsystems in a strict order,
and the platform layer sits in the middle of it:

```
memory system → logging → input → events
      ↓
platform_startup(&app_state->platform, name, x, y, w, h)
      ↓
platform_get_device_pixel_ratio(&app_state->platform)   // Retina scaling
      ↓
renderer system (needs the platform state to create the VkSurface)
      ↓
clock_set_platform_state(&app_state->platform)          // timing comes last
```

Shutdown is roughly the reverse. `platform_shutdown` closes the window, releases
the Cocoa objects and frees `internal_state`. The clock keeps working until
`platform_shutdown` runs; after that `clock_set_platform_state` must not be
relied upon.

---

## The clock & timing

### The `clock` type (`engine/src/core/clock.h`)

```c
typedef struct clock {
    f64 start_time;   // platform timestamp when clock_start() was called
    f64 elapsed;      // seconds elapsed since clock_start(), updated by clock_update()
} clock;
```

Semantics:

- `clock_start` — records the current platform timestamp and resets `elapsed`.
- `clock_update` — refreshes `elapsed` (no effect on a never-started clock).
  Call it whenever you want to read the current elapsed value; it does **not**
  tick on its own.
- `clock_stop` — clears `start_time` but **keeps** `elapsed`, so a stopped clock
  still reports how long it ran.

All values are `f64` **seconds**.

### How time is sourced: `clock_set_platform_state`

The clock does not talk to the OS itself. `clock.c` keeps a single
`g_platform_state` pointer, set once by `application_create` right after the
platform (and therefore the timers) exist:

```c
clock_set_platform_state(&app_state->platform);
```

Every `clock_update`/`clock_start` then resolves the current time through:

```c
f64 platform_get_absolute_time(platform_state* plat_state);
```

If `clock_set_platform_state` was never called, `g_platform_state` is NULL and
the clock's calls take the platform's **uninitialized fallback path** (see
below). This is intentional so unit tests can run headless, but it means the
timeline is different from a fully started app — don't mix the two.

### Per-OS implementations

| Platform | Mechanism | Epoch |
|---|---|---|
| macOS | `mach_absolute_time()` scaled by `mach_timebase_info` | **Engine start** (`state->start_time`, captured in `platform_startup`) |
| Windows | `QueryPerformanceCounter()` × `clock_frequency` | System boot |
| Linux | `clock_gettime(CLOCK_MONOTONIC)` | System boot |

macOS details (`platform_macos.m`):

1. `platform_startup` caches `mach_timebase_info(&state->timebase_info)` and
   `state->start_time = mach_absolute_time()`.
2. `platform_get_absolute_time` computes
   `(mach_absolute_time() - start_time) * numer / (denom * 1e9)` — i.e. **seconds
   since engine start**, converted from Mach ticks via the timebase ratio.
3. If `plat_state` is NULL (or `internal_state` is unset) the function falls
   back to raw `mach_absolute_time()` with its own lazily-initialized timebase.
   That value is relative to system boot rather than engine start, so only use
   it for measuring *intervals* (this is what the test suite does).

Windows and Linux ignore `plat_state` entirely — their timers need no state.

### How the main loop uses it (`application_run`)

```
clock_start(&app_state->clock)
each frame:
    platform_pump_messages()                    // events first
    clock_update(&app_state->clock)             // now: elapsed = time since start
    delta = clock.elapsed - last_time           // seconds since previous frame
    frame_start = clock_get_absolute_time(...)  // stamp the frame
    game->update(delta); game->render(delta);
    renderer_draw_frame(&packet);
    frame_elapsed = now - frame_start
    if (frame faster than 1/60 s) → optional platform_sleep() to hit the target
    input_update(delta)                         // input state copied last
    last_time = clock.elapsed
```

Notes:

- `delta` is a `f64` seconds value cast to `f32` for the game API — on very long
  sessions the cast is harmless at 60 FPS magnitudes.
- Frame limiting is currently compiled off (`limit_frames = false` in
  `application.c`), so the loop runs unthrottled.
- `EVENT_CODE_RESIZED` handling also timestamps nothing itself; it only carries
  sizes (see window system below).

---

## The window system (macOS)

`platform_macos.m` is the richest backend. Its `internal_state` bundles
everything Cocoa-related:

```objc
typedef struct internal_state {
    NSWindow           *window;
    KohiContentView    *content_view;      // NSView hosting the Metal layer
    KohiWindowDelegate *window_delegate;
    CAMetalLayer       *metal_layer;       // the Vulkan swapchain's render target
    b8                  quit_flagged;
    mach_timebase_info_data_t timebase_info;  // clock support (see above)
    u64                       start_time;
} internal_state;
```

### `platform_startup` — what actually gets created

1. **App shell:** `[NSApplication sharedApplication]` with
   `NSApplicationActivationPolicyRegular`, plus a minimal menubar containing a
   `Quit <name>` item (`⌘Q`).
2. **Window:** an `NSWindow` with `Titled | Closable | Miniaturizable |
   Resizable`, created at `(x, y, width, height)` and titled with the app name.
   Failure here aborts startup.
3. **Delegate:** a `KohiWindowDelegate` is installed; it holds a pointer to
   `internal_state.quit_flagged` (used by the close flow, below).
4. **Content view:** `KohiContentView` (an `NSView` subclass) is set as the
   window's content view. In its `initWithFrame:` it creates a `CAMetalLayer`
   with:
   - `pixelFormat = MTLPixelFormatBGRA8Unorm`
   - `framebufferOnly = YES`
   - the view's bounds as its frame
5. **Retina scaling:** `contentsScale` and `drawableSize = view bounds ×
   mainScreen backingScaleFactor` are set up front. The **drawable size — not
   the point size — is what the swapchain ultimately matches**, so this is where
   "Retina" enters the pipeline.
6. **Focus & input:** `makeKeyAndOrderFront`, `setAcceptsMouseMovedEvents:YES`,
   and the content view becomes first responder (it must be, or key events never
   arrive).
7. **Clock seed:** timebase + `start_time` are captured here (see [clock](#the-clock--timing)).

### The CAMetalLayer → Vulkan connection

The layer created above is handed to Vulkan through
`platform_create_vulkan_surface`, which calls `vkCreateMetalSurfaceEXT`
(`VK_EXT_metal_surface`) with `pLayer = state->metal_layer`. Two MoltenVK
specifics live in `platform_get_required_extension_names`:

- `VK_KHR_portability_enumeration` — required with SDK ≥ 1.3.216, paired with
  `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` in `vulkan_backend.c`.
  Without it, `vkCreateInstance` returns `VK_ERROR_INCOMPATIBLE_DRIVER`.
- `VK_EXT_metal_surface` — the WSI extension exposing `vkCreateMetalSurfaceEXT`.

So the render chain is:

```
NSWindow → KohiContentView → CAMetalLayer → VkSurfaceKHR → VkSwapchainKHR
```

### Window close flow

```
user clicks the red close button
   → [KohiWindowDelegate windowShouldClose:] logs, sets quit_flagged = true,
     returns NO (the window itself is NOT closed yet)
   → next platform_pump_messages() returns false
   → application_run() sets is_running = false and enters shutdown
```

This design means the engine, not AppKit, owns the shutdown sequence — the
window stays visible until `platform_shutdown` actually closes it.

### Window resize flow

```
user drags the resize handle
   → [KohiWindowDelegate windowDidResize:]
       contentRect = [window contentRectForFrameRect:window.frame]
       ctx.data.u16[0/1] = content size × window.backingScaleFactor   (pixels)
   → event_fire(EVENT_CODE_RESIZED, ...)
   → application_on_resized → game->on_resize + renderer_on_resized
```

In parallel, `KohiContentView` keeps the Metal layer in sync from three hooks:

| Hook | What it does |
|---|---|
| `setFrameSize:` | Updates layer frame + `drawableSize` on every layout change |
| `updateLayer` | Re-syncs the layer frame (with `wantsUpdateLayer = YES`) |
| `viewDidChangeBackingProperties` | Screen moved/zoomed → recompute `contentsScale` and `drawableSize` from the *window's* screen (falls back to `mainScreen`) |

All reported sizes are in **backing pixels** (points × scale factor), which is
the unit the renderer wants for viewport/framebuffer sizing.

---

## Event pumping & input

### The message pump

`platform_pump_messages` drains the AppKit queue without blocking:

```objc
while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                   untilDate:[NSDate distantPast]   // ← never nil
                                      inMode:NSDefaultRunLoopMode
                                     dequeue:YES]) != nil) {
    [NSApp sendEvent:event];
    [NSApp updateWindows];
}
return !state->quit_flagged;
```

**Why `distantPast` and not `nil`:** a `nil` date would block until the next
user event arrives, freezing the game loop during idle time. `distantPast`
returns immediately with whatever is pending — pump everything, then run the
frame.

It's called once per frame at the top of `application_run`; a `false` return is
the app's quit signal.

### Input delivery (macOS)

`KohiContentView` is the first responder, so AppKit delivers input to it, and it
translates events into engine calls:

| AppKit callback | Engine call |
|---|---|
| `keyDown:` / `keyUp:` | `input_process_key(translate_keycode([event keyCode]), …)` |
| `flagsChanged:` | Modifier press state detected per keycode (⇧ 0x38/0x3C, ⌃ 0x3B/0x3E, ⌥ 0x3A/0x3D, ⌘ 0x37/0x36) |
| `mouseDown:`/`rightMouseDown:`/`otherMouseDown:` (button 2) | `input_process_button(LEFT/RIGHT/MIDDLE, true)` |
| matching `*Up:` | `input_process_button(…, false)` |
| `mouseMoved:` + all drag variants | `input_process_mouse_move(x, y)` |
| `scrollWheel:` | `input_process_mouse_wheel(±1)` — precise (trackpad) deltas scaled ×0.1, sign only |

Details worth knowing:

- **Coordinate flip:** AppKit mouse coordinates are bottom-left origin;
  the engine expects top-left. `_processMoveEvent` does
  `y = viewHeight − eventY`.
- `translate_keycode` maps Apple hardware keycodes (position-independent, not
  characters — so it works on any layout) to engine `keys` values; unknown
  keycodes return 0 and are dropped by the caller.
- Mouse position is given in *points*; window-resize events deliver *pixels*
  (scaled). These two spaces are intentionally different: input is UI-space,
  resize events are framebuffer-space.
- The input system itself (`core/input.c`) only stores state; `input_update` at
  the end of each frame copies it into the "previous frame" state for edge
  detection (`input_was_key_down` etc.). That's why `input_update` is called
  **after** the game update — see the note in `application_run`.

---

## Other platform services

- **Memory:** `platform_allocate`/`platform_free` (plain `malloc`/`free`;
  `aligned` requests use 16-byte `posix_memalign`), plus
  `zero/copy/set_memory` wrappers around `memset`/`memcpy`. These are the
  primitives under `kallocate`/`kfree` and the engine's tagged stats.
- **Console:** `platform_console_write` prints to `stdout`,
  `platform_console_write_error` to `stderr`, both with ANSI colour codes (the
  `colour` index maps into a table of SGR sequences) and an explicit `fflush`.
- **Sleep:** `nanosleep` (macOS/Linux), `Sleep` (Win32). Blocking, main-thread
  only — used by the (currently disabled) frame limiter.
- **Pixel ratio:** `platform_get_device_pixel_ratio` returns the window screen's
  `backingScaleFactor` (macOS); the application multiplies its logical window
  size by this right after startup.

---

## Known quirks & pitfalls

- **Only the macOS timer uses `plat_state`.** Win32/Linux `platform_get_absolute_time`
  ignore it. The macOS fallback path (NULL state) yields boot-relative time
  instead of engine-relative time — fine for intervals, wrong for absolute
  comparisons across the two paths.
- **`clock_update` is pull-based.** Forgetting to call it before reading
  `elapsed` gives you a stale value. The main loop calls it exactly once per
  frame.
- **`windowShouldClose:` returns NO.** Closing the window is only a *request*
  flag; actual window destruction happens in `platform_shutdown`. Don't
  "fix" the delegate to return YES — that would tear down the CAMetalLayer
  while Vulkan still renders into it.
- **Retina enters at two places:** content-view layer updates (`drawableSize`)
  and `windowDidResize` event payloads (× backing scale). Game-space input
  positions are *not* scaled — if you mix them with framebuffer sizes, divide by
  the pixel ratio first.
- **Mouse-moved events only flow** because of `setAcceptsMouseMovedEvents:YES`
  plus the content view being first responder. If input ever goes dead after a
  refactor, check both.
- **`platform_pump_messages` must use `[NSDate distantPast]`.** See the note
  above; `nil` turns the loop into an event-blocking UI app.
- The Vulkan portability dance (`VK_KHR_portability_enumeration` + instance
  flag + MoltenVK headers before system headers) is a hard requirement on
  macOS; the include order comment at the top of `platform_macos.m` explains
  the header precedence issue.
