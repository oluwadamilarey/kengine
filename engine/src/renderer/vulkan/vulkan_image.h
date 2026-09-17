#pragma once

#include "vulkan_types.inl"

void vulkan_image_create(
    vulkan_context* context,
    VkImageType image_type,
    u32 width,
    u32 height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    b32 create_view,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image);

/**
 * @brieftransitions from an old_layout to a new_layout
 * for the given image.
 * This is used to transition an image from one layout to another,
 * such as from undefined to shader read-only.
 */
void vulkan_image_transition_layout(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer,
    vulkan_image* image,
    VkFormat format,
    VkImageLayout old_layout,
    VkImageLayout new_layout);

/**
 * copies data in a buffer to a provided image.
 * @param context The vulkan context.
 * @param image
 * @param buffer The buffer to copy from.
 *
 */

void vulkan_image_copy_from_buffer(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer,
    vulkan_image* image,
    VkBuffer buffer
    // u32 width,
    // u32 height
);

void vulkan_image_view_create(
    vulkan_context* context,
    VkFormat format,
    vulkan_image* image,
    VkImageAspectFlags aspect_flags);

void vulkan_image_destroy(
    vulkan_context* context,
    vulkan_image* image);