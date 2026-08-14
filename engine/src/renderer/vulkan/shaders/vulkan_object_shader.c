#include "vulkan_object_shader.h"
#include "core/logger.h"
#include "math/math_types.h"
#include "renderer/vulkan/vulkan_shader_utils.h"
#include "renderer/vulkan/vulkan_pipeline.h"
#include "core/kmemory.h"

#include "math/kmath.h"
#define BUILTIN_SHADER_NAME_OBJECT "Builtin.ObjectShader"

b8 vulkan_object_shader_create(vulkan_context* context, vulkan_object_shader* out_shader) {
    // Shader module init per stage
    char stage_type_strs[OBJECT_SHADER_STAGE_COUNT][5] = {"vert", "frag"};
    VkShaderStageFlagBits stage_types[OBJECT_SHADER_STAGE_COUNT] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) {
        if (!create_shader_module(context, BUILTIN_SHADER_NAME_OBJECT, stage_type_strs[i], stage_types[i], i, out_shader->stages)) {
            KERROR("Unable to create %s shader module for '%s'.", stage_type_strs[i], BUILTIN_SHADER_NAME_OBJECT);
            return false;
        }
    }

    // Descriptors
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (f32)context->framebuffer_height;
    viewport.width = (f32)context->framebuffer_width;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = context->framebuffer_width;
    scissor.extent.height = context->framebuffer_height;

    // Attributes
    u32 offset = 0;
    const i32 attribute_count = 1;
    VkVertexInputAttributeDescription attribute_description[attribute_count];

    // Position
    VkFormat formats[attribute_count] = {
        VK_FORMAT_R32G32B32_SFLOAT};

    u64 sizes[attribute_count] = {
        sizeof(vec3)};

    for (u32 i = 0; i < attribute_count; ++i) {
        attribute_description[i].location = i;
        attribute_description[i].binding = 0;
        attribute_description[i].format = formats[i];
        attribute_description[i].offset = offset;
        offset += sizes[i];
    }

    // stages
    // Note: should match the number of shader->stages
    VkPipelineShaderStageCreateInfo stage_create_infos[OBJECT_SHADER_STAGE_COUNT];
    kzero_memory(stage_create_infos, sizeof(stage_create_infos));
    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) {
        stage_create_infos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_create_infos[i].stage = stage_types[i];
        stage_create_infos[i].module = out_shader->stages[i].handle;
        stage_create_infos[i].pName = "main";
    }

    if (!vulkan_graphics_pipeline_create(
            context,
            &context->main_renderpass,
            attribute_count,
            attribute_description,
            0,
            0,
            OBJECT_SHADER_STAGE_COUNT,
            stage_create_infos,
            viewport,
            scissor,
            false,
            &out_shader->pipeline)) {
        KERROR("Unable to create pipeline for '%s'.", BUILTIN_SHADER_NAME_OBJECT);
        return false;
    }

    return true;
};

void vulkan_object_shader_destroy(vulkan_context* context, struct vulkan_object_shader* shader) {
    vulkan_pipeline_destroy(context, &shader->pipeline);
    // destroy shader modules
    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i) {
        vkDestroyShaderModule(context->device.logical_device, shader->stages[i].handle, context->allocator);
        shader->stages[i].handle = 0;
    }
};

void vulkan_object_shader_use(vulkan_context* context, struct vulkan_object_shader* shader) {

};