#include "vulkan_profiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/clock.h"
#include "core/logger.h"

// Swapchains rarely exceed 4 images. Slots beyond this are simply not timed.
#define PROFILER_MAX_SLOTS 8
#define PROFILER_MAX_WINDOW 4096

typedef struct profiler_state {
    b8 initialized;
    b8 enabled;
    b8 gpu_enabled;

    struct platform_state* plat_state;

    u32 warmup_frames;
    u32 window_frames;
    u64 frames_seen;

    // CPU timing for the frame currently being recorded.
    f64 cpu_begin_time;
    f64 stall_begin_time;
    f64 stall_accum_ms;

    // GPU timing.
    VkQueryPool query_pool;
    b8 slot_submitted[PROFILER_MAX_SLOTS];
    f64 timestamp_period_ns;
    u64 timestamp_mask;

    // Sample windows.
    f32 cpu_ms[PROFILER_MAX_WINDOW];
    f32 stall_ms[PROFILER_MAX_WINDOW];
    f32 gpu_ms[PROFILER_MAX_WINDOW];
    u32 cpu_count;
    u32 gpu_count;
    u32 window_index;
} profiler_state;

static profiler_state s;

b8 vulkan_profiler_env_flag(const char* name, b8 default_value) {
    const char* value = getenv(name);
    if (!value || !value[0]) {
        return default_value;
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "off") == 0) {
        return false;
    }
    return true;
}

u32 vulkan_profiler_env_u32(const char* name, u32 default_value, u32 min_value, u32 max_value) {
    const char* value = getenv(name);
    if (!value || !value[0]) {
        return default_value;
    }
    char* end = 0;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value) {
        return default_value;
    }
    if (parsed < min_value) {
        return min_value;
    }
    if (parsed > max_value) {
        return max_value;
    }
    return (u32)parsed;
}

u32 vulkan_profiler_stress_draws(void) {
    static u32 cached = 0;
    if (cached == 0) {
        cached = vulkan_profiler_env_u32("KENGINE_STRESS_DRAWS", 1, 1, VULKAN_OBJECT_MAX_OBJECTS_COUNT);
    }
    return cached;
}

b8 vulkan_profiler_enabled(void) {
    return s.enabled;
}

b8 vulkan_profiler_initialize(vulkan_context* context, struct platform_state* plat_state) {
    memset(&s, 0, sizeof(s));
    s.initialized = true;
    s.plat_state = plat_state;
    s.enabled = vulkan_profiler_env_flag("KENGINE_PROFILE", false);

    if (!s.enabled) {
        return true;
    }

    s.warmup_frames = vulkan_profiler_env_u32("KENGINE_PROFILE_WARMUP", 120, 0, 100000);
    s.window_frames = vulkan_profiler_env_u32("KENGINE_PROFILE_WINDOW", 300, 10, PROFILER_MAX_WINDOW);

    vulkan_device* device = &context->device;

    // Timestamps need valid bits on the graphics queue family.
    VkQueueFamilyProperties families[16];
    u32 family_count = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &family_count, families);

    u32 valid_bits = 0;
    if (device->graphics_queue_index >= 0 && (u32)device->graphics_queue_index < family_count) {
        valid_bits = families[device->graphics_queue_index].timestampValidBits;
    }

    if (valid_bits == 0) {
        KWARN("Profiler: graphics queue reports no timestamp support, GPU timing disabled.");
    } else {
        VkQueryPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        pool_info.queryCount = PROFILER_MAX_SLOTS * 2;
        VkResult result = vkCreateQueryPool(device->logical_device, &pool_info, context->allocator, &s.query_pool);
        if (result != VK_SUCCESS) {
            KWARN("Profiler: vkCreateQueryPool failed (%d), GPU timing disabled.", result);
        } else {
            s.gpu_enabled = true;
            s.timestamp_period_ns = (f64)device->properties.limits.timestampPeriod;
            s.timestamp_mask = valid_bits >= 64 ? ~0ull : ((1ull << valid_bits) - 1ull);
        }
    }

    KINFO("Profiler: ON device='%s' warmup=%u window=%u gpu_timing=%s timestamp_period=%.3fns stress_draws=%u bind_cache=%s draw_logs=%s",
          device->properties.deviceName,
          s.warmup_frames,
          s.window_frames,
          s.gpu_enabled ? "yes" : "no",
          s.timestamp_period_ns,
          vulkan_profiler_stress_draws(),
          vulkan_profiler_env_flag("KENGINE_BIND_CACHE", false) ? "on" : "off",
          vulkan_profiler_env_flag("KENGINE_DRAW_LOGS", true) ? "on" : "off");
    return true;
}

void vulkan_profiler_shutdown(vulkan_context* context) {
    if (s.query_pool) {
        vkDestroyQueryPool(context->device.logical_device, s.query_pool, context->allocator);
        s.query_pool = 0;
    }
    s.enabled = false;
    s.gpu_enabled = false;
}

static int compare_f32(const void* a, const void* b) {
    f32 x = *(const f32*)a;
    f32 y = *(const f32*)b;
    return (x > y) - (x < y);
}

// Sorts data in place and logs mean / p50 / p95 / max.
static void report_series(const char* label, f32* data, u32 count, char* out, u32 out_size) {
    if (count == 0) {
        snprintf(out, out_size, "%s n/a", label);
        return;
    }
    qsort(data, count, sizeof(f32), compare_f32);
    f64 sum = 0.0;
    for (u32 i = 0; i < count; ++i) {
        sum += data[i];
    }
    u32 p50 = (count - 1) * 50 / 100;
    u32 p95 = (count - 1) * 95 / 100;
    snprintf(out, out_size, "%s mean %.3f p50 %.3f p95 %.3f max %.3f",
             label, sum / count, data[p50], data[p95], data[count - 1]);
}

static void flush_window(void) {
    char cpu_line[128];
    char stall_line[128];
    char gpu_line[128];
    u32 cpu_count = s.cpu_count;
    u32 gpu_count = s.gpu_count;

    report_series("cpu", s.cpu_ms, cpu_count, cpu_line, sizeof(cpu_line));
    report_series("stall", s.stall_ms, cpu_count, stall_line, sizeof(stall_line));
    report_series("gpu", s.gpu_ms, gpu_count, gpu_line, sizeof(gpu_line));

    KINFO("Profile window %u (%u frames, ms): %s | %s | %s",
          s.window_index, cpu_count, cpu_line, stall_line, gpu_line);

    s.window_index++;
    s.cpu_count = 0;
    s.gpu_count = 0;
}

void vulkan_profiler_cpu_frame_begin(void) {
    if (!s.enabled) {
        return;
    }
    s.cpu_begin_time = clock_get_absolute_time(s.plat_state);
    s.stall_accum_ms = 0.0;
}

void vulkan_profiler_stall_begin(void) {
    if (!s.enabled) {
        return;
    }
    s.stall_begin_time = clock_get_absolute_time(s.plat_state);
}

void vulkan_profiler_stall_end(void) {
    if (!s.enabled) {
        return;
    }
    s.stall_accum_ms += (clock_get_absolute_time(s.plat_state) - s.stall_begin_time) * 1000.0;
}

void vulkan_profiler_cpu_frame_end(void) {
    if (!s.enabled) {
        return;
    }
    s.frames_seen++;
    if (s.frames_seen <= s.warmup_frames) {
        return;
    }

    f64 elapsed_ms = (clock_get_absolute_time(s.plat_state) - s.cpu_begin_time) * 1000.0;
    s.cpu_ms[s.cpu_count] = (f32)elapsed_ms;
    s.stall_ms[s.cpu_count] = (f32)s.stall_accum_ms;
    s.cpu_count++;

    if (s.cpu_count >= s.window_frames) {
        flush_window();
    }
}

void vulkan_profiler_gpu_frame_begin(vulkan_context* context, VkCommandBuffer command_buffer, u32 slot) {
    if (!s.enabled || !s.gpu_enabled || slot >= PROFILER_MAX_SLOTS) {
        return;
    }

    // Collect the result of this slot's previous use, if the GPU has finished with it.
    // This is non-blocking: an unavailable result is skipped rather than waited on.
    if (s.slot_submitted[slot] && s.frames_seen > s.warmup_frames) {
        u64 results[4] = {0, 0, 0, 0};  // {start, start available, end, end available}
        VkResult result = vkGetQueryPoolResults(
            context->device.logical_device,
            s.query_pool,
            slot * 2,
            2,
            sizeof(results),
            results,
            2 * sizeof(u64),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if (result == VK_SUCCESS && results[1] != 0 && results[3] != 0 && s.gpu_count < PROFILER_MAX_WINDOW) {
            u64 ticks = (results[2] - results[0]) & s.timestamp_mask;
            s.gpu_ms[s.gpu_count++] = (f32)((f64)ticks * s.timestamp_period_ns / 1.0e6);
        }
    }

    // Resets must be recorded outside a render pass.
    vkCmdResetQueryPool(command_buffer, s.query_pool, slot * 2, 2);
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s.query_pool, slot * 2);
}

void vulkan_profiler_gpu_frame_end(vulkan_context* context, VkCommandBuffer command_buffer, u32 slot) {
    (void)context;
    if (!s.enabled || !s.gpu_enabled || slot >= PROFILER_MAX_SLOTS) {
        return;
    }
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s.query_pool, slot * 2 + 1);
    s.slot_submitted[slot] = true;
}
