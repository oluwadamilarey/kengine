#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

typedef struct global_uniform_object {
    mat4 projection;   // 64 bytes - projection matrix for the camera
    mat4 view;         // 64 bytes - view matrix for the camera
    mat4 m_reserved0;  // 64 bytes - reserved for future use
    mat4 m_reserved1;  // 64 bytes - reserved for future use
} global_uniform_object;

#include "vulkan/vulkan_types.inl"

typedef enum renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX,
    RENDERER_BACKEND_TYPE_METAL,
} renderer_backend_type;

typedef struct object_uniform_object {
    vec4 diffuse_color;   // 16 bytes - diffuse color for the object
    vec4 specular_color;  // 16 bytes - specular color for the object
    f32 shininess;        // 4 bytes - shininess factor for the object
    f32 m_reserved0;      // 4 bytes - reserved for future use
    f32 m_reserved1;      // 4 bytes - reserved for future use
    f32 m_reserved2;      // 4 bytes - reserved for future use
} object_uniform_object;

typedef struct geometry_render_data {
    mat4 model;             // 64 bytes - model matrix for the object
    u32 object_id;          // 48 bytes - uniform buffer object for the object
    texture* textures[16];  // 8 bytes - pointer to the texture associated with the object
} geometry_render_data;

typedef struct renderer_backend {
    struct platform_state* plat_state;
    u64 frame_number;

    // pointer to the default diffuse texture that is used when an object does not have a texture assigned to it.
    texture* default_diffuse;
    b8 (*initialize)(struct renderer_backend* backend, const char* application_name, struct platform_state* plat_state);
    void (*shutdown)(struct renderer_backend* backend);
    void (*resized)(struct renderer_backend* backend, u16 width, u16 height);
    b8 (*begin_frame)(struct renderer_backend* backend, f32 delta_time);
    void (*update_global_state)(mat4 projection, mat4 view, vec3 view_position, vec4 ambient_colour, i32 mode);
    b8 (*end_frame)(struct renderer_backend* backend, f32 delta_time);
    void (*update_object)(geometry_render_data* data);

    void (*create_texture)(
        const char* name,
        b8 auto_release,
        i32 width,
        i32 height,
        i32 channel_count,
        b8 has_transparency,
        const u8* pixels,
        struct texture* out_texture);

    void (*destroy_texture)(struct texture* texture);
} renderer_backend;

typedef struct render_packet {
    f32 delta_time;
} render_packet;
