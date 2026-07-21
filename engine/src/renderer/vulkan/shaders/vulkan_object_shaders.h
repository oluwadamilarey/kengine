#pragma once

#include "renderer/vulkan/vulkan_types.inl"

#include "renderer/renderer_types.inl"

// b8 vulkan_object_shader_create(vulkan_object_shaders* shaders, vulkan_context* context, const char* vertex_shader_path, const char* fragment_shader_path);

// void vulkan_object_shader_destroy(vulkan_object_shaders* shaders, vulkan_context* context);

// void vulkan_object_shader_use(vulkan_object_shaders* shaders, vulkan_context* context);

b8 vulkan_object_shaders_create(vulkan_context* context, vulkan_object_shader* out_shader);

void vulkan_object_shader_destroy(vulkan_context* context, struct vulkan_object_shader* shader);

void vulkan_object_shader_use(vulkan_context* context, struct vulkan_object_shader* shader);