#pragma once

#include "vulkan_types.inl"
#include "defines.h"

/**
 * @brief AI Generated stub function for vulkan_renderpass_create.
 *
 *
 */
// void vulkan_renderpass_create(
//     vulkan_context* context,
//     vulkan_renderpass* out_renderpass,
//     VkFormat color_format,
//     VkFormat depth_format,
//     b8 clear_color,
//     b8 clear_depth);
void vulkan_renderpass_create(
    vulkan_context* context,
    vulkan_renderpass* out_renderpass,
    f32 x, f32 y, f32 w,
    f32 h, f32 r, f32 g,
    f32 b, f32 a,
    f32 depth, u32 stencil);

void vulkan_renderpass_destroy(
    vulkan_context* context,
    vulkan_renderpass* renderpass);

b8 vulkan_renderpass_begin(
    // vulkan_context* context,
    vulkan_renderpass* renderpass,
    vulkan_command_buffer* command_buffer,
    VkFramebuffer framebuffer);

b8 vulkan_renderpass_end(
    // vulkan_context* context,
    vulkan_renderpass* renderpass,
    vulkan_command_buffer* command_buffer);
