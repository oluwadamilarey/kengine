#include "vulkan_command_buffer.h"

#include "core/logger.h"

// Allocates a command buffer from the specified command pool, and initializes its state. Only valid for primary command buffers.
void vulkan_command_buffer_allocate(
    vulkan_context* context,
    VkCommandPool command_pool,
    b8 is_primary,
    vulkan_command_buffer* out_command_buffer) {
    kzero_memory(out_command_buffer, sizeof(vulkan_command_buffer));

    VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate_info.commandPool = command_pool;
    allocate_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocate_info.commandBufferCount = 1;
    allocate_info.pNext = 0;

    out_command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    VK_CHECK(vkAllocateCommandBuffers(context->device.logical_device, &allocate_info, &out_command_buffer->handle));
}

// Frees the command buffer's resources and resets its state. Only valid for primary command buffers that have already been allocated.
void command_buffer_free(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer) {
    if (command_buffer->handle) {
        vkFreeCommandBuffers(context->device.logical_device, pool, 1, &command_buffer->handle);
        command_buffer->handle = VK_NULL_HANDLE;
        command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    }
}

// Begins recording commands to the command buffer. Only valid for primary command buffers that are in the ready state.
void command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_render_pass_continue,
    b8 is_simultaneous_use) {
    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = 0;
    if (is_single_use) {
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }
    if (is_render_pass_continue) {
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }
    if (is_simultaneous_use) {
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    }
    begin_info.pInheritanceInfo = 0;  // Only relevant for secondary command buffers.

    VK_CHECK(vkBeginCommandBuffer(command_buffer->handle, &begin_info));
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
}

// Ends recording of the command buffer, transitioning it to the ready state. Only valid for primary command buffers that are currently recording.
void vulkan_command_buffer_end(
    vulkan_command_buffer* command_buffer) {
    VK_CHECK(vkEndCommandBuffer(command_buffer->handle));
    command_buffer->state = COMMAND_BUFFER_STATE_READY;
}

// Updates the command buffer's state to submitted. This should be called after submitting the command buffer to a queue, and is used to track when a command buffer is in-flight and cannot be reset or recorded to again until it has finished executing on the GPU.
void vulkan_command_buffer_update_submitted(
    vulkan_command_buffer* command_buffer) {
    command_buffer->state = COMMAND_BUFFER_STATE_SUBMITTED;
}

// Resets the command buffer to the ready state, allowing it to be recorded again. Only valid for primary command buffers that have already been submitted.
void vulkan_command_buffer_reset(
    vulkan_command_buffer* command_buffer) {
    VK_CHECK(vkResetCommandBuffer(command_buffer->handle, 0));
    command_buffer->state = COMMAND_BUFFER_STATE_READY;
}

// Convenience function for allocating and beginning a single-use command buffer in one call.
void vulkan_command_buffer_allocate_and_begin_single_use(
    vulkan_context* context,
    VkCommandPool command_pool,
    b8 is_primary,
    vulkan_command_buffer* out_command_buffer) {
    vulkan_command_buffer_allocate(context, command_pool, is_primary, out_command_buffer);
    vulkan_command_buffer_begin(out_command_buffer, TRUE, FALSE, FALSE);
}

// Convenience function for ending and freeing a single-use command buffer in one call.
void vulkan_command_buffer_end_and_free_single_use(
    vulkan_context* context,
    VkCommandPool command_pool,
    vulkan_command_buffer* command_buffer) {
    vulkan_command_buffer_end(command_buffer);
    vulkan_command_buffer_free(context, command_pool, command_buffer);

    // submit the command buffer to the graphics queue and wait for it to finish executing before returning. This is typically used for short-lived command buffers that perform one-off operations like copying buffers or transitioning image layouts.
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;

    VK_CHECK(vkQueueSubmit(context->device.graphics_queue, 1, &submit_info, VK_NULL_HANDLE));

    // Wait for the command buffer to finish executing before returning, since the caller may immediately free resources that the command buffer references after this function returns.
    VK_CHECK(vkQueueWaitIdle(context->device.graphics_queue));
}

