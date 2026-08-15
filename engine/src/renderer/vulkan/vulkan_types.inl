#pragma once

#include "defines.h"
#include "core/asserts.h"
#include "core/logger.h"

#include <vulkan/vulkan.h>
// checks the given expression's return value against VK_SUCCESS.
#define VK_CHECK(expr)                         \
    do {                                       \
        VkResult result__ = (expr);            \
        if (result__ != VK_SUCCESS) {          \
            KERROR("Vulkan error %d at %s:%d", \
                   result__,                   \
                   __FILE__,                   \
                   __LINE__);                  \
        }                                      \
    } while (0)

typedef struct vulkan_image {
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    u32 width;
    u32 height;
} vulkan_image;

typedef struct vulkan_pipeline {
    VkPipeline handle;
    VkPipelineLayout pipeline_layout;
} vulkan_pipeline;

typedef struct vulkan_fence {
    VkFence handle;
    b8 is_signaled;
} vulkan_fence;

typedef enum vulkan_renderpass_state {
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED,
} vulkan_renderpass_state;

typedef struct vulkan_renderpass {
    VkRenderPass handle;
    VkFramebuffer framebuffer;

    f32 x, y, w, h;
    f32 r, g, b, a;

    f32 depth;
    u32 stencil;
    vulkan_renderpass_state state;
} vulkan_renderpass;

typedef struct vulkan_framebuffer {
    VkFramebuffer handle;
    u32 attachment_count;
    VkImageView* attachments;
    vulkan_renderpass* renderpass;
} vulkan_framebuffer;

typedef struct vulkan_swapchain {
    VkSurfaceFormatKHR image_format;
    u8 max_frames_in_flight;
    VkSwapchainKHR handle;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    u32 image_count;
    VkImage* images;
    VkImageView* views;  // darray
    vulkan_image depth_attachment;
    vulkan_framebuffer* framebuffers;  // darray
} vulkan_swapchain;

typedef enum vulkan_command_buffer_state {
    COMMAND_BUFFER_STATE_READY,
    COMMAND_BUFFER_STATE_RECORDING,
    COMMAND_BUFFER_STATE_SUBMITTED,
    COMMAND_BUFFER_STATE_NOT_ALLOCATED,
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,
    COMMAND_BUFFER_STATE_RECORDING_ENDED,
} vulkan_command_buffer_state;

typedef struct vulkan_command_buffer {
    VkCommandBuffer handle;
    vulkan_command_buffer_state state;
} vulkan_command_buffer;

typedef struct vulkan_buffer {
    u64 total_size;
    VkBuffer handle;
    VkBufferUsageFlagBits usage;
    b8 is_locked;
    VkDeviceMemory memory;
    i32 memory_index;
    b8 bind_on_create;
    u32 memory_property_flags;
} vulkan_buffer;

typedef struct vulkan_swapchain_support_info {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;  // darray
    u32 format_count;
    VkPresentModeKHR* present_modes;  // darray
    u32 present_mode_count;
} vulkan_swapchain_support_info;

typedef struct vulkan_device {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_swapchain_support_info swapchain_support;

    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;
    VkCommandPool transfer_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;

    VkFormat depth_format;
} vulkan_device;

typedef struct vulkan_shader_stage {
    VkShaderModuleCreateInfo create_info;
    VkShaderModule handle;
    VkPipelineShaderStageCreateInfo shader_stage_create_info;
} vulkan_shader_stage;

#define OBJECT_SHADER_STAGE_COUNT 2

typedef struct vulkan_object_shader {
    vulkan_shader_stage stages[OBJECT_SHADER_STAGE_COUNT];
    vulkan_pipeline pipeline;
} vulkan_object_shader;

typedef struct vulkan_context {
    u32 framebuffer_width;
    u32 framebuffer_height;
    u64 framebuffer_size_generation;
    u64 framebuffer_size_last_generation;
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

    // Always present regardless of build config so vulkan_context has a single,
    // stable layout across every translation unit. A struct member gated on
    // _DEBUG is a silent ABI break if even one .c/.m file in the build is
    // compiled with a different _DEBUG state than the rest — every field
    // after it (including find_memory_index, below) then lands at the wrong
    // byte offset in that file's view of the struct. That produces exactly
    // the "call through a seemingly-valid pointer that faults at 0x0" bug:
    // the misaligned read/call lands on some other, zero-initialized field
    // instead. Leave this unused outside debug builds rather than omitting it.
    VkDebugUtilsMessengerEXT debug_messenger;

    vulkan_device device;
    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;

    vulkan_buffer object_vertex_buffer;
    vulkan_buffer object_index_buffer;

    vulkan_command_buffer* graphics_command_buffers;  // darray
    VkSemaphore* image_available_semaphores;          // darray
    VkSemaphore* queue_complete_semaphores;           // darray
    u32 in_flight_fence_count;

    vulkan_fence* in_flight_fences;
    vulkan_fence** images_in_flight;

    u32 image_index;
    u32 current_frame;

    vulkan_object_shader object_shader;

    u64 geometry_vertex_offset;
    u64 geometry_index_offset;

    b8 recreating_swapchain;

    i32 (*find_memory_index)(u32 type_filter, u32 property_flags);
} vulkan_context;