#include "vulkan_swapchain.h"

#include "core/logger.h"
#include "core/kmemory.h"
#include "containers/darray.h"
#include "vulkan_device.h"

void create(vulkan_context* context,
            u32 width,
            u32 height,
            vulkan_swapchain* out_swapchain) {
    // Choose surface format
    VkSurfaceFormatKHR chosen_format = context->device.swapchain_support.formats[0];
    for (u32 i = 0; i < context->device.swapchain_support.format_count; ++i) {
        VkSurfaceFormatKHR current_format = context->device.swapchain_support.formats[i];
        if (current_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            current_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = current_format;
            break;
        }
    }

    out_swapchain->image_format = chosen_format;

    // Choose extent
    VkExtent2D extent;
    if (context->device.swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
        extent = context->device.swapchain_support.capabilities.currentExtent;
    } else {
        extent.width = width;
        extent.height = height;

        if (extent.width < context->device.swapchain_support.capabilities.minImageExtent.width) {
            extent.width = context->device.swapchain_support.capabilities.minImageExtent.width;
        } else if (extent.width > context->device.swapchain_support.capabilities.maxImageExtent.width) {
            extent.width = context->device.swapchain_support.capabilities.maxImageExtent.width;
        }

        if (extent.height < context->device.swapchain_support.capabilities.minImageExtent.height) {
            extent.height = context->device.swapchain_support.capabilities.minImageExtent.height;
        } else if (extent.height > context->device.swapchain_support.capabilities.maxImageExtent.height) {
            extent.height = context->device.swapchain_support.capabilities.maxImageExtent.height;
        }
    }

    out_swapchain->extent = extent;

    // Choose image count
    u32 image_count = context->device.swapchain_support.capabilities.minImageCount + 1;
    if (context->device.swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > context->device.swapchain_support.capabilities.maxImageCount) {
        image_count = context->device.swapchain_support.capabilities.maxImageCount;
    }
    out_swapchain->image_count = image_count;

    // Create swapchain
    VkSwapchainCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = context->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = context->device.swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;  // Always supported
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(
        context->device.logical_device,
        &create_info,
        context->allocator,
        &out_swapchain->swapchain);
    if (result != VK_SUCCESS) {
        KFATAL("Failed to create Vulkan swapchain. VkResult: %d", result);
        return;
    }
    KINFO("Vulkan swapchain created successfully.");
}

void destroy(vulkan_context* context, vulkan_swapchain* swapchain) {
    // Destroy image views
    if (swapchain->image_views) {
        for (u32 i = 0; i < swapchain->image_count; ++i) {
            if (swapchain->image_views[i]) {
                vkDestroyImageView(
                    context->device.logical_device,
                    swapchain->image_views[i],
                    context->allocator);
            }
        }
        darray_free(swapchain->image_views, MEMORY_TAG_RENDERER);
        swapchain->image_views = NULL;
    }

    // Destroy swapchain
    if (swapchain->swapchain) {
        vkDestroySwapchainKHR(
            context->device.logical_device,
            swapchain->swapchain,
            context->allocator);
        swapchain->swapchain = VK_NULL_HANDLE;
    }

    // Free images array
    if (swapchain->images) {
        darray_free(swapchain->images, MEMORY_TAG_RENDERER);
        swapchain->images = NULL;
    }

    swapchain->image_count = 0;
};

void vulkan_swapchain_create(vulkan_context* context, u32 width, u32 height, vulkan_swapchain* out_swapchain) {
    create(context, width, height, out_swapchain);
}

void vulkan_swapchain_recreate(vulkan_context* context, vulkan_swapchain* swapchain, u32 width, u32 height) {
    destroy(context, swapchain);
    create(context, width, height, swapchain);
}

void vulkan_swapchain_destroy(vulkan_context* context, vulkan_swapchain* swapchain) {
    destroy(context, swapchain);
}

b8 vulkan_swapchain_acquire_next_image(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32* out_image_index) {
    VkResult result = vkAcquireNextImageKHR(
        context->device.logical_device,
        swapchain->swapchain,
        timeout_ns,
        image_available_semaphore,
        fence,
        out_image_index);

    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        return TRUE;
    } else {
        KERROR("Failed to acquire next swapchain image. VkResult: %d", result);
        return FALSE;
    }
}

void vulkan_swapchain_present(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    VkQueue present_queue,
    VkQueue graphics_queue,
    u32 present_image_index,
    VkSemaphore render_complete_semaphore) {
    //return the image to the swapchain for presentation.
    //because it is a queue we need a semaphore for it
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->swapchain;
    present_info.pImageIndices = &present_image_index;
    present_info.pResults = NULL;  // Optional
    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result != VK_SUCCESS) {
        KERROR("Failed to present swapchain image. VkResult: %d", result);
    }
}

