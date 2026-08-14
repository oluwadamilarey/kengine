#pragma once

#include "defines.h"
#include "core/asserts.h"

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
    /** @brief The internal pipeline handle. */
    VkPipeline handle;
    /** @brief The pipeline layout. */
    VkPipelineLayout pipeline_layout;
    /** @brief Indicates the topology types used by this pipeline. See primitive_topology_type.*/
    // primitive_topology_type_bits supported_topology_types;
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
    // framebuffers used for on-screen rendering, one for each swapchain image.
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
    // vertex , fragment
    vulkan_shader_stage stages[OBJECT_SHADER_STAGE_COUNT];
    vulkan_pipeline pipeline;
} vulkan_object_shader;

typedef struct vulkan_context {
    // The framebuffer's current width.
    u32 framebuffer_width;
    // The framebuffer's current height.
    u32 framebuffer_height;
    // generation counter for framebuffer size changes, used to detect when the swapchain needs to be recreated.
    u64 framebuffer_size_generation;

    // The framebuffer size generation at the last swapchain creation, used to detect when the swapchain needs to be recreated.
    u64 framebuffer_size_last_generation;
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
#if defined(_DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    vulkan_device device;
    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;

    vulkan_buffer object_vertex_buffer;
    vulkan_buffer object_index_buffer;

    // darray of command buffers, one for each frame in flight.
    vulkan_command_buffer* graphics_command_buffers;
    // darray
    VkSemaphore* image_available_semaphores;
    // darray
    VkSemaphore* queue_complete_semaphores;
    u32 in_flight_fence_count;

    vulkan_fence* in_flight_fences;
    vulkan_fence** images_in_flight;

    // VkFence* in_flight_fences;
    u32 image_index;
    u32 current_frame;

    vulkan_object_shader object_shader;

    u64 geometry_vertex_offset;
    u64 geometry_index_offset;

    b8 recreating_swapchain;

    i32 (*find_memory_index)(u32 type_filter, u32 property_flags);
} vulkan_context;
