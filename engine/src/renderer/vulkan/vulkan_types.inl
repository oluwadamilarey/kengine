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

typedef struct vulkan_swapchain {
    VkSurfaceFormatKHR image_format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    u32 image_count;
    VkImage* images;           // darray
    VkImageView* image_views;  // darray
} vulkan_swapchain;

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

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;
} vulkan_device;

typedef struct vulkan_context {
    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;
#if defined(_DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    vulkan_device device;
} vulkan_context;