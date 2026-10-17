# Kengine

Kengine is a small, C-based game engine with a Vulkan-powered renderer and a simple “testbed” client
application. It is structured as a shared engine library (`libengine`) plus separate executables for
the testbed and unit tests.

Current focus areas:

- Cross‑platform platform layer (macOS, Linux, Windows).
- Vulkan rendering backend (via MoltenVK on macOS).
- Basic engine subsystems: memory, events, input, timing, logging.
- Simple 3D camera + orbit controls and textured geometry in the testbed.

---

## Repository Layout

Top level:

- `engine/` — Engine library source code and build scripts.
- `testbed/` — Sample application using the engine.
- `tests/` — Unit test runner and test code.
- `assets/` — Shared engine/testbed assets (shaders, textures).
- `bin/` — Build outputs (shared library, testbed/tests executables, logs, copied assets).
- `obj/` — Intermediate object files.
- `build-*.sh`, `build-all-*.sh`, `Makefile.*.mak` — Platform‑specific build scripts for engine,
  testbed, and tests.

Key top‑level files:

- `Makefile.engine.macos.mak` — macOS build rules for `libengine.dylib` using Vulkan SDK.
- `Makefile.testbed.macos.mak`, `Makefile.tests.macos.mak` — Similar for testbed/tests.
- `LICENSE` — Project license.
- `build-all-macos.sh`, `clean-all-macos.sh` — Convenience scripts to (re)build/clean everything.

---

## Engine Overview (`engine/src`)

High‑level responsibilities:

- Owns subsystems: memory, logging, events, input, timing, renderer.
- Provides a `game` interface (from `game_types.h`) that client apps implement.
- Runs the main loop and forwards events, timing, and rendering to the game.

### Core

`engine/src/core`:

- `application.c/.h`
  - Creates and owns `application_state`.
  - Sets up:
    - Linear allocator (`systems_allocator`).
    - Memory system (`initialize_memory`, `shutdown_memory`).
    - Logging system (`initialize_logging`).
    - Event system (`event_initialize`, `event_register`, `event_shutdown`).
    - Input system (`input_initialize`, `input_update`, `input_shutdown`).
    - Platform/window (`platform_startup`, `platform_pump_messages`, `platform_shutdown`).
    - Renderer (`renderer_system_initialize`, `renderer_system_shutdown`).
    - Frame timing (`clock_*`).
  - Calls into the game:
    - `game->initialize`, `game->update`, `game->render`, `game->on_resize`.
  - Main loop:
    - Pumps platform messages.
    - Updates clock, computes `delta`.
    - Calls game `update` and `render`.
    - Builds a `render_packet` and calls `renderer_draw_frame`.
    - Optionally sleeps to approximate a 60 FPS target.
    - Handles input state update and window resize events.
- `event.*` — Central event bus with registration, firing, and some predefined event codes
  (quit, resize, input, debug events like `EVENT_CODE_DEBUG0`).
- `input.*` — Keyboard/mouse state, edge detection, and helpers:
  - `input_is_key_down`, `input_was_key_down`, `input_is_button_down`, etc.
- `clock.*` — Engine timing utilities, tracking elapsed time and absolute timestamps.
- `logger.*` — Log categories/macros (`KTRACE`, `KDEBUG`, `KINFO`, `KWARN`, `KERROR`, `KFATAL`).
- `kmemory.*` — Engine memory system, allocation tracking, and tags.
- `kstring.*` — String helpers (e.g., `string_format` used in texture loading).

### Math

`engine/src/math`:

- `kmath.*`
  - Scalar math helpers (sin/cos/atan2, degrees↔radians).
  - Vector and matrix types and functions (e.g., `vec3_add`, `mat4_mul`, `mat4_inverse`,
    `mat4_perspective`, `mat4_translation`, `mat4_forward/right/left/backward`).
- `math_types.h`
  - Vector/matrix typedefs (`vec3`, `mat4`, etc.).

Used by:

- Camera + orbit logic in `testbed/src/game.c`.
- Renderer (`renderer_frontend.c`) for view/projection math.

### Memory

`engine/src/memory`:

- `linear_allocator.*`
  - Simple linear/arena allocator used by `application_state` to carve out memory for subsystems
    (logging, renderer, memory system, etc.).
  - The engine expects the application to pre‑allocate a large block
    (`systems_allocator_total_size = 64 MB`) for subsystem state.

### Platform

`engine/src/platform`:

- `platform.h`
  - Cross‑platform interface for:
    - Window creation and teardown.
    - Event pumping.
    - Sleeping.
    - Device pixel ratio/query (`platform_get_device_pixel_ratio`).
- `platform_macos.m`
  - macOS implementation, including:
    - Window/view setup (Cocoa/Metal integration for MoltenVK).
    - Keyboard/mouse input events.
    - High‑resolution timers.
- `platform_linux.c`, `platform_win32.c`
  - Non‑macOS stubs/implementations (in progress or less frequently used).
- `filesystem.*`
  - File utilities (currently used for asset loading and potentially logging).

### Renderer

`engine/src/renderer`:

- Frontend (`renderer_frontend.c/.h`):
  - Engine‑facing render API; owns `renderer_system_state`:
    - `renderer_backend backend;`
    - `mat4 projection`, `mat4 view;`
    - `f32 near_clip`, `far_clip;`
    - Textures: `default_texture`, `test_diffuse`, etc.
  - Initialization:
    - Registers for debug events (`EVENT_CODE_DEBUG0`) to swap textures.
    - Creates Vulkan backend (`renderer_backend_create`) with `RENDERER_BACKEND_TYPE_VULKAN`.
    - Sets a default perspective projection (currently fixed 1280x720).
    - Computes an initial view matrix from a backward translation.
    - Builds a procedural checkerboard default texture on CPU and uploads it to GPU.
    - Loads `assets/textures/paving.png` as a test diffuse texture via `stb_image`.
  - Texture helpers:
    - `create_texture` / `load_texture`:
      - Load from `assets/textures/<name>.png` with `stb_image` (flipped vertically).
      - Detect alpha transparency.
      - Call into backend to create/destroy GPU textures (`renderer_create_texture`,
        `renderer_destroy_texture`).
      - Manage a `generation` counter and auto‑release old textures.
  - Frame API:
    - Provides `renderer_set_view` (set from `game_state->view`).
    - Provides `renderer_draw_frame(render_packet* packet)` used in the main loop.
  - Shutdown:
    - Unregisters debug event handlers.
    - Destroys textures.
    - Calls backend `shutdown`.

- Backend (`renderer_backend.*`, `renderer_types.inl`):
  - Abstract interface implemented by the Vulkan backend: init, shutdown, begin/end frame,
    create/destroy textures, etc.
  - Holds function pointers and per‑frame state (`frame_number`, swapchain, etc.).

- Vulkan backend (`renderer/vulkan`):
  - `vulkan_backend.*` — High‑level Vulkan renderer orchestration (swapchain, render passes,
    command submission).
  - `vulkan_device.*` — Physical/logical device selection and setup.
  - `vulkan_swapchain.*` — Swapchain creation, resize handling, frame acquisition/presentation.
  - `vulkan_image.*` — Image creation, layout transitions, views.
  - `vulkan_buffer.*` — GPU buffers (vertex/index/uniform).
  - `vulkan_pipeline.*` — Graphics pipelines (shaders, fixed‑function state).
  - `vulkan_renderpass.*` — Render pass definition (attachments, subpasses).
  - `vulkan_command_buffer.*` — Command buffer allocation/recording helpers.
  - `vulkan_fence.*`, `vulkan_utils.*`, `vulkan_types.inl` — Synchronization and utilities.
  - `vulkan_shader_utils.*` — Shader module loading helpers.
  - `vulkan_platform.h` — OS‑specific Vulkan plumbing hooks.

  Shader sources used by the backend live in:

  - `assets/shaders/Builtin.ObjectShader.vert.glsl`
  - `assets/shaders/Builtin.ObjectShader.frag.glsl`

### Resources

`engine/src/resources`:

- `resource_types.h` — Basic resource type definitions (textures, etc.), consumed by renderer and
  potentially by future resource systems.

### Vendor

`engine/src/vendor`:

- `stb_image.h` — Bundled image loader; compiled and used from `renderer_frontend.c`.

---

## Testbed (`testbed/src`)

The testbed is a small example game/app built on the engine.

- `entry.c`
  - Implements `create_game(game* out_game)` (from `entry.h`).
  - Fills `app_config`:
    - Window origin `(100, 100)`.
    - Size `1280x720`.
    - Window title `"Kohi Engine Testbed"`.
  - Binds game callbacks:
    - `initialize` → `game_initialize`.
    - `update` → `game_update`.
    - `render` → `game_render`.
    - `on_resize` → `game_on_resize`.
  - Allocates the `game_state` from the memory system.
- `game.c/.h`
  - Owns `game_state`, including:
    - Camera position/euler angles.
    - Orbit target and radius.
    - Cached `view` matrix and a `camera_view_dirty` flag.
  - Camera helpers:
    - `recalculate_view_matrix` — Builds view from euler+position using `mat4_euler_xyz` and
      `mat4_translation`, then inverts.
    - `camera_orbit`, `camera_orbit_mouse` — Orbit target via angle or mouse deltas.
    - `camera_roll`, `camera_zoom_dolly`, `camera_yaw`, `camera_pitch`.
  - Input handling in `game_update`:
    - Debug keys:
      - `M` — Print allocation stats via `get_memory_alloc_count()`.
      - `T` — Fire `EVENT_CODE_DEBUG0` to cycle textures.
    - Camera rotation (WASD + arrow keys).
    - Camera translation along view axes (W/S + Q/E + V/X).
    - Camera roll (`R`), zoom/dolly (`Z`/`C`).
    - Axis‑aligned snap views (numpad `1/3/7`) — front/side/top.
    - Y‑axis orbit using `[` and `]`.
    - Trackpad orbit:
      - `Option` (`KEY_LALT`) + left‑click drag.
      - Uses previous and current mouse positions only when both modifier and click were
        down on the previous frame as well, to avoid camera jumps.
  - After updating camera state:
    - Calls `recalculate_view_matrix`.
    - Passes new view to renderer via `renderer_set_view`.
  - `game_render` is currently a stub returning `true` — actual drawing is handled in the
    renderer backend based on view/projection and internal scene/geometry.
  - `game_on_resize` is currently empty — resize handling is delegated to renderer and
    application code (`renderer_on_resized`, `app_state->game_inst->on_resize`).

---

## Tests (`tests/src`)

- `main.c`
  - Entry point for running engine unit tests.
- `test_manager.*`, `expect.h`
  - Minimal test harness: registering tests, running them, providing assertion macros.
- `memory/…`
  - Memory system tests (coverage of linear allocator, `kmemory`, etc.).

The tests build into a `tests` executable in `bin/` and likely rely on the same engine code
linked into `libengine`.

---

## Assets (`assets/`)

- `assets/shaders/`
  - `Builtin.ObjectShader.vert.glsl`
  - `Builtin.ObjectShader.frag.glsl`
  - Used by Vulkan backend for the default pipeline.
- `assets/textures/`
  - `cobblestone.png`
  - `paving.png`
  - `paving2.png`
  - Default/test textures; `renderer_frontend` loads `"paving"` by name into `test_diffuse`,
    and runtime debug events can swap among them.

At build or runtime, textures and shaders are accessed via relative paths such as
`assets/textures/<name>.png`, so the working directory must be set to the repo root or have
assets copied/symlinked into `bin/assets`.

---

## Build & Run

### Prerequisites (macOS)

- Clang (via Xcode Command Line Tools).
- Vulkan SDK with MoltenVK (e.g. installed under `~/VulkanSDK/<version>/macOS`).
- Environment:
  - `VULKAN_SDK` must point either to:
    - `<install_root>/macOS`, or
    - `<install_root>` where `include/` and `lib/` exist.

Example:

```bash
export VULKAN_SDK="$HOME/VulkanSDK/1.4.350.0/macOS"
```

The macOS engine Makefile will derive:

- `VULKAN_INCLUDE_DIR` from `$VULKAN_SDK/include` or `$VULKAN_SDK/macOS/include`.
- `VULKAN_LIB_DIR` from `$VULKAN_SDK/lib` or `$VULKAN_SDK/macOS/lib`.

### Build Everything (macOS)

From the repo root:

```bash
./build-all-macos.sh
```

This will:

- Build `engine` → `bin/libengine.dylib`.
- Build `testbed` → `bin/testbed`.
- Build `tests` → `bin/tests`.
- Copy/link any needed assets into `bin/assets` (via post‑build scripts).

To clean:

```bash
./clean-all-macos.sh
```

### Individual Builds

Engine only:

```bash
make -f Makefile.engine.macos.mak all
```

Testbed only:

```bash
make -f Makefile.testbed.macos.mak all
```

Tests only:

```bash
make -f Makefile.tests.macos.mak all
```

(Equivalent `.linux.mak` / `.windows.mak` exist for other platforms, but may require additional
toolchain/SDK setup.)

### Running

From the repo root after building:

```bash
cd bin
./testbed         # run the demo app
./tests           # run unit tests
```

Ensure that `assets/` is accessible (either in `../assets` from `bin`, or copied into `bin/assets`
per the post‑build scripts).

---

## Current State & Notes

- Engine runs with a Vulkan backend on macOS (MoltenVK; recent changes in
  `vulkan_backend.c`, `vulkan_types.inl`, and renderer frontend indicate active work).
- Default camera + orbit navigation is implemented and working in the testbed.
- Texture loading is working via `stb_image` with runtime swapping between a few sample textures.
- Resize handling:
  - Window resize events are handled by `application_on_resized`, which updates
    `game->on_resize` and calls `renderer_on_resized`.
  - `game_on_resize` is currently a no‑op; per‑game resize behavior can be added there.
- The renderer currently assumes a fixed initial aspect ratio (1280x720) for the projection
  matrix; dynamic aspect ratio updates on resize would be a likely next improvement.

---

## Next Steps / Possible TODOs

- Update projection matrix on window resize based on actual framebuffer size.
- Flesh out `game_render` with explicit draw calls/scene objects instead of relying purely on
  backend defaults.
- Expand resource system to include materials, meshes, and shader management beyond the built‑in
  object shader.
- Add more unit tests (renderer, math, platform abstractions).
- Improve platform parity for Linux/Windows builds and Vulkan surface creation.
