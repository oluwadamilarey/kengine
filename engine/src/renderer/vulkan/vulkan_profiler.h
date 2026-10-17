#pragma once

#include "vulkan_types.inl"

struct platform_state;

/*
 * Optional GPU/CPU frame profiler and A/B switches for the Vulkan backend.
 *
 * Everything is driven by environment variables that are read once, so the build
 * and the vulkan_context layout are identical whether profiling is on or off.
 *
 *   KENGINE_PROFILE=1            enable the profiler                       (default 0)
 *   KENGINE_PROFILE_WARMUP=120   frames to discard before sampling         (default 120)
 *   KENGINE_PROFILE_WINDOW=300   frames per reported window, max 4096      (default 300)
 *   KENGINE_STRESS_DRAWS=N       draw N small quads per frame, max 1024    (default 1)
 *   KENGINE_BIND_CACHE=1         skip redundant pipeline/vertex/index binds (default 0)
 *   KENGINE_DRAW_LOGS=0          silence the per-draw KINFO lines in the
 *                                object shader                              (default 1)
 *
 * Defaults reproduce the engine's existing behaviour exactly.
 *
 * Reported per window (ms): cpu = begin_frame start to end_frame end, stall = time
 * blocked on fences and image acquire, gpu = timestamp query across the whole
 * command buffer. Each is shown as mean / p50 / p95 / max.
 */

b8 vulkan_profiler_initialize(vulkan_context* context, struct platform_state* plat_state);
void vulkan_profiler_shutdown(vulkan_context* context);
b8 vulkan_profiler_enabled(void);

// Environment helpers. env_flag treats "0", "false" and "off" as false; any other
// non-empty value as true. env_u32 clamps to [min_value, max_value].
b8 vulkan_profiler_env_flag(const char* name, b8 default_value);
u32 vulkan_profiler_env_u32(const char* name, u32 default_value, u32 min_value, u32 max_value);

// Number of quads the frontend should draw per frame (KENGINE_STRESS_DRAWS, >= 1).
u32 vulkan_profiler_stress_draws(void);

// CPU timing. Call cpu_frame_begin at the top of begin_frame and cpu_frame_end after
// present. Wrap anything that blocks (fence waits, image acquire) in stall_begin/end.
void vulkan_profiler_cpu_frame_begin(void);
void vulkan_profiler_cpu_frame_end(void);
void vulkan_profiler_stall_begin(void);
void vulkan_profiler_stall_end(void);

// GPU timing. gpu_frame_begin must be recorded outside a render pass, right after the
// command buffer begins. gpu_frame_end is recorded after the render pass ends.
// slot is the swapchain image index (matches how command buffers are indexed).
void vulkan_profiler_gpu_frame_begin(vulkan_context* context, VkCommandBuffer command_buffer, u32 slot);
void vulkan_profiler_gpu_frame_end(vulkan_context* context, VkCommandBuffer command_buffer, u32 slot);
