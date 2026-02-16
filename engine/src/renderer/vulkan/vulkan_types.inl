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

    // The current state of the command buffer.
    vulkan_command_buffer_state state;
} vulkan_command_buffer;

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

typedef struct vulkan_context {
    // The framebuffer's current width.
    u32 framebuffer_width;
    // The framebuffer's current height.
    u32 framebuffer_height;
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
#if defined(_DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    vulkan_device device;
    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;

    // darray of command buffers, one for each frame in flight.
    vulkan_command_buffer* graphics_command_buffers;

    u32 image_index;
    u32 current_frame;
    b8 recreating_swapchain;

    i32 (*find_memory_index)(u32 type_filter, u32 property_flags);
} vulkan_context;
