#pragma once

#include "vulkan_types.inl"
#include "defines.h"

void vulkan_command_buffer_allocate(
    vulkan_context* context,
    VkCommandPool command_pool,
    b8 is_primary,
    vulkan_command_buffer* out_command_buffer);

void vulkan_command_buffer_free(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer);

void vulkan_command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_render_pass_continue,
    b8 is_simultaneous_use);

void vulkan_command_buffer_end(
    vulkan_command_buffer* command_buffer);

void vulkan_command_buffer_update_submitted(
    vulkan_command_buffer* command_buffer);

void vulkan_command_buffer_reset(
    vulkan_command_buffer* command_buffer);