#include "vulkan_renderpass.h"

#include "core/logger.h"
#include "core/kmemory.h"

void vulkan_renderpass_create(
    vulkan_context* context,
    vulkan_renderpass* out_renderpass,
    f32 x, f32 y, f32 w, f32 h,
    f32 r, f32 g, f32 b, f32 a,
    f32 depth,
    u32 stencil) {
    out_renderpass->x = x;
    out_renderpass->y = y;
    out_renderpass->w = w;
    out_renderpass->h = h;

    out_renderpass->r = r;
    out_renderpass->g = g;
    out_renderpass->b = b;
    out_renderpass->a = a;

    out_renderpass->depth = depth;
    out_renderpass->stencil = stencil;

    // Main subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Attachments TODO: make this configurable.
    u32 attachment_description_count = 2;
    VkAttachmentDescription attachment_descriptions[attachment_description_count];

    // Color attachment
    VkAttachmentDescription color_attachment;
    color_attachment.format = context->swapchain.image_format.format;  // TODO: configurable
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // Do not expect any particular layout before render pass starts.
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Transitioned to after the render pass
    color_attachment.flags = 0;

    attachment_descriptions[0] = color_attachment;

    VkAttachmentReference color_attachment_reference;
    color_attachment_reference.attachment = 0;  // Attachment description array index
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    // Depth attachment, if there is one
    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = context->device.depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    attachment_descriptions[1] = depth_attachment;

    // Depth attachment reference
    VkAttachmentReference depth_attachment_reference;
    depth_attachment_reference.attachment = 1;
    depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // TODO: other attachment types (input, resolve, preserve)

    // Depth stencil data.
    subpass.pDepthStencilAttachment = &depth_attachment_reference;

    // Input from a shader
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = 0;

    // Attachments used for multisampling colour attachments
    subpass.pResolveAttachments = 0;

    // Attachments not used in this subpass, but must be preserved for the next.
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = 0;

    // Render pass dependencies. TODO: make this configurable.
    VkSubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    // Render pass create.
    VkRenderPassCreateInfo render_pass_create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_create_info.attachmentCount = attachment_description_count;
    render_pass_create_info.pAttachments = attachment_descriptions;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;
    render_pass_create_info.pNext = 0;
    render_pass_create_info.flags = 0;

    VK_CHECK(vkCreateRenderPass(
        context->device.logical_device,
        &render_pass_create_info,
        context->allocator,
        &out_renderpass->handle));
}

void vulkan_renderpass_destroy(vulkan_context* context, vulkan_renderpass* renderpass) {
    if (renderpass && renderpass->handle) {
        vkDestroyRenderPass(context->device.logical_device, renderpass->handle, context->allocator);
        renderpass->handle = 0;
    }
}

/**
 * @brief Begins a Vulkan render pass, recording vkCmdBeginRenderPass into the
 *        given command buffer and transitioning its state machine accordingly.
 *
 * Render area and clear values are driven entirely by the renderpass struct,
 * so the caller controls them by mutating that struct before calling this
 * (as recreate_swapchain does when it patches x/y/w/h after a resize).
 *
 * Attachment layout:
 *   [0] colour — VkClearColorValue        (r, g, b, a) in linear space
 *   [1] depth/stencil — VkClearDepthStencilValue (depth=1.0, stencil=0)
 *
 * Subpass contents mode is INLINE — all draw commands are recorded directly
 * into this primary command buffer. Secondary command buffer execution
 * (VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS) is not used.
 *
 * @param command_buffer  Primary command buffer in COMMAND_BUFFER_STATE_RECORDING.
 *                        State is advanced to COMMAND_BUFFER_STATE_IN_RENDER_PASS
 *                        on return, gating any subsequent renderpass-dependent
 *                        commands (draws, pipeline binds, etc.).
 * @param renderpass      Renderpass to begin. Supplies the VkRenderPass handle,
 *                        render area rect (x, y, w, h), and clear values
 *                        (r, g, b, a, depth, stencil).
 * @param frame_buffer    The VkFramebuffer compatible with @p renderpass whose
 *                        attachments back this render. Must match the image
 *                        acquired from the swapchain for this frame.
 */
void vulkan_renderpass_begin(
    vulkan_command_buffer* command_buffer,
    vulkan_renderpass* renderpass,
    VkFramebuffer frame_buffer) {

    VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin_info.renderPass  = renderpass->handle;
    begin_info.framebuffer = frame_buffer;

    /* Render area defines the region of the framebuffer that will be affected.
     * Pixels outside this rect are undefined after the pass — keep it tight.
     * On a resize this rect is patched in recreate_swapchain before we're called. */
    begin_info.renderArea.offset.x      = renderpass->x;
    begin_info.renderArea.offset.y      = renderpass->y;
    begin_info.renderArea.extent.width  = renderpass->w;
    begin_info.renderArea.extent.height = renderpass->h;

    /* VkClearValue is a union (color | depthStencil). Zero the whole array first
     * to avoid hitting undefined bits in whichever union member is NOT written —
     * the Vulkan spec leaves those bits unspecified but some validation layers
     * will flag uninitialised memory if you don't.                             */
    VkClearValue clear_values[2];
    kzero_memory(clear_values, sizeof(VkClearValue) * 2);

    /* [0] Colour attachment clear — maps to the swapchain colour image.
     * Values must be in linear colour space; sRGB conversion (if any) is
     * handled by the swapchain surface format, not here.                       */
    clear_values[0].color.float32[0] = renderpass->r;
    clear_values[0].color.float32[1] = renderpass->g;
    clear_values[0].color.float32[2] = renderpass->b;
    clear_values[0].color.float32[3] = renderpass->a;

    /* [1] Depth/stencil attachment clear.
     * depth=1.0 fills to the far plane (standard for a [0,1] depth range).
     * stencil=0 resets all stencil bits — adjust if stencil effects are added. */
    clear_values[1].depthStencil.depth   = renderpass->depth;
    clear_values[1].depthStencil.stencil = renderpass->stencil;

    begin_info.clearValueCount = 2;
    begin_info.pClearValues    = clear_values;

    /* VK_SUBPASS_CONTENTS_INLINE: draw calls are embedded directly in this
     * primary command buffer. No secondary command buffers are dispatched
     * via vkCmdExecuteCommands inside this pass.                               */
    vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    /* Advance the state machine so upstream callers (and future assertions)
     * can verify that renderpass-scoped commands (vkCmdDraw*, vkCmdBindPipeline,
     * push constants, etc.) are only issued while a pass is active.            */
    command_buffer->state = COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void vulkan_renderpass_end(vulkan_command_buffer* command_buffer, vulkan_renderpass* renderpass) {
    vkCmdEndRenderPass(command_buffer->handle);
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
}