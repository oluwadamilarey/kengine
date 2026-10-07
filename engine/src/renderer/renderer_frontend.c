#include "renderer_frontend.h"
#include "renderer_backend.h"

#include "core/logger.h"
#include "core/kmemory.h"
#include "math/kmath.h"

#include "core/kstring.h" x
#include "core/event.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

typedef struct renderer_system_state {
    renderer_backend backend;
    mat4 projection;
    mat4 view;
    f32 near_clip;
    f32 far_clip;
    texture default_texture;
    texture test_diffuse;
} renderer_system_state;

// Backend render context. Points into memory owned by application_state's
// linear allocator — this module never allocates or frees it itself.
static renderer_system_state* state_ptr;

void create_texture(texture* texture) {
    kzero_memory(texture, sizeof(texture));
    texture->generation = INVALID_ID;
}

b8 load_texture(const char* texture_name, texture* t) {
    char* format_string = "assets/textures/%s.%s";
    const i32 required_channel_count = 4;
    stbi_set_flip_vertically_on_load(true);
    i32 width, height, channel_count;
    char full_file_path[512];

    // TODO: Determine the file extension based on the file type. For now, we will assume all textures are PNGs. try differnt extensions
    string_format(full_file_path, format_string, texture_name, "png");

    // use a temp texture to load data
    texture temp_texture;

    u8* data = stbi_load(full_file_path, (i32*)&temp_texture.width, (i32*)&temp_texture.height, (i32*)&channel_count, required_channel_count);

    temp_texture.channel_count = required_channel_count;
    temp_texture.has_transparency = (channel_count == 4) ? true : false;

    if (data) {
        u32 current_generation = t->generation;
        t->generation = INVALID_ID;  // mark the texture as invalid until it is fully loaded
        u64 total_size = temp_texture.width * temp_texture.height * required_channel_count;
        // check for transperency in the texture data
        b32 has_transparency = false;

        for (u64 i = 0; i < total_size; i += required_channel_count) {
            u8 alpha = data[i + 3];
            if (alpha < 255) {
                temp_texture.has_transparency = true;
                break;
            }
        }

        if (stbi_failure_reason()) {
            KWARN("Failed to load texture: %s. Reason: %s", full_file_path, stbi_failure_reason());
            return false;
        }

        // acquire internal resources and upload to the GPU
        renderer_create_texture(
            texture_name,
            false,  // auto_release
            temp_texture.width,
            temp_texture.height,
            temp_texture.channel_count,
            temp_texture.has_transparency,
            data,
            &temp_texture);

        // take a copy of the old texture
        texture old = *t;
        *t = temp_texture;
        t->generation = current_generation + 1;  // increment the generation to indicate that

        // destroy the old texture if it was valid
        if (old.generation != INVALID_ID) {
            renderer_destroy_texture(&old);
        }

        stbi_image_free(data);
        return true;

        create_texture(t);
        t->width = temp_texture.width;
    } else {
        if (stbi_failure_reason()) {
            KWARN("Failed to load texture: %s. Reason: %s", full_file_path, stbi_failure_reason());
        } else {
            KWARN("Failed to load texture: %s. Unknown reason.", full_file_path);
        }
        return false;
    }
}

b8 event_on_debug_event(u16 code, void* sender, void* listener_inst, event_context data){

}

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name, struct platform_state* plat_state) {
    *memory_requirement = sizeof(renderer_system_state);
    if (state == 0) {
        // First pass: caller only wants the size, so it can allocate the block.
        return true;
    }

    state_ptr = state;

    // take a pointer to the default textures for use on the backend
    state_ptr->backend.default_diffuse = &state_ptr->default_texture;

    if (!renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, plat_state, &state_ptr->backend)) {
        KFATAL("Failed to create renderer backend.");
        return false;
    }

    state_ptr->backend.frame_number = 0;

    if (!state_ptr->backend.initialize(&state_ptr->backend, application_name, plat_state)) {
        KFATAL("Renderer backend failed to initialize. Shutting down.");
        return false;
    }

    // TODO: make configurable / read from app_config.
    state_ptr->near_clip = 0.1f;
    state_ptr->far_clip = 1000.0f;
    state_ptr->projection = mat4_perspective(deg_to_rad(45.0f), 1280.0f / 720.0f, state_ptr->near_clip, state_ptr->far_clip);

    state_ptr->view = mat4_translation((vec3){0, 0, -30.0f});
    state_ptr->view = mat4_inverse(state_ptr->view);

    KTRACE("creating default texture...");
    const u32 tex_dimension = 256;

    const u32 channels = 4;
    const u32 pixel_count = tex_dimension * tex_dimension;
    // Allocate a contiguous pixel buffer (RGBA8)
    u8* pixels = (u8*)kallocate(pixel_count * channels * sizeof(u8), MEMORY_TAG_RENDERER);
    kset_memory(pixels, 255, pixel_count * channels * sizeof(u8));

    // each pixel
    for (u64 row = 0; row < tex_dimension; ++row) {
        for (u64 col = 0; col < tex_dimension; ++col) {
            u64 index = (row * tex_dimension) + col;
            u64 index_bpp = index * channels;
            if (row % 2) {
                if (col % 2) {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            } else {
                if (!(col % 2)) {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
        }
    }

    renderer_create_texture(
        "default",
        false,
        tex_dimension,
        tex_dimension,
        channels,
        true,  // has_transparency
        pixels,
        &state_ptr->default_texture);

    // manually set the texure generation to INVALID_ID since this is the first texture being created
    state_ptr->default_texture.generation = INVALID_ID;

    // TODO: load other textures here, or provide a way for the application to load them.
    create_texture(&state_ptr->test_diffuse);
    if (!load_texture("test_diffuse", &state_ptr->test_diffuse)) {
        KWARN("Failed to load test_diffuse texture.");
    }

    return true;
}

void renderer_system_shutdown() {
    if (state_ptr) {
        renderer_destroy_texture(&state_ptr->default_texture);
        renderer_destroy_texture(&state_ptr->test_diffuse);
        state_ptr->backend.shutdown(&state_ptr->backend);
        state_ptr = 0;
    }
    // NOTE: no kfree here — this block belongs to application_state's
    // linear allocator and is torn down with it, not individually.
}

void renderer_on_resized(u16 width, u16 height) {
    if (state_ptr) {
        state_ptr->projection = mat4_perspective(
            deg_to_rad(45.0f), (f32)width / (f32)height, state_ptr->near_clip, state_ptr->far_clip);
        state_ptr->backend.resized(&state_ptr->backend, width, height);
    } else {
        KWARN("renderer backend does not exist to accept resize: %i %i", width, height);
    }
}

b8 renderer_begin_frame(f32 delta_time) {
    if (!state_ptr) {
        return false;
    }
    return state_ptr->backend.begin_frame(&state_ptr->backend, delta_time);
}

b8 renderer_end_frame(f32 delta_time) {
    b8 result = state_ptr->backend.end_frame(&state_ptr->backend, delta_time);
    state_ptr->backend.frame_number++;
    return result;
}

b8 renderer_draw_frame(render_packet* packet) {
    if (renderer_begin_frame(packet->delta_time)) {
        state_ptr->backend.update_global_state(state_ptr->projection, state_ptr->view, vec3_zero(), vec4_one(), 0);

        static f32 angle = 0.01f;
        angle += 0.01f;

        quat rotation = quat_from_axis_angle(vec3_forward(), angle, false);
        mat4 model = quat_to_rotation_matrix(rotation, vec3_zero());
        geometry_render_data data = {};
        data.object_id = 0;
        data.model = model;
        data.textures[0] = &state_ptr->test_diffuse;
        state_ptr->backend.update_object(&data);

        b8 result = renderer_end_frame(packet->delta_time);
        if (!result) {
            KERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}

void renderer_create_texture(const char* name,
                             b8 auto_release,
                             i32 width,
                             i32 height,
                             i32 channel_count,
                             b8 has_transparency,
                             const u8* pixels,
                             struct texture* out_texture) {
    state_ptr->backend.create_texture(name, auto_release, width, height, channel_count, has_transparency, pixels, out_texture);
};

void renderer_destroy_texture(struct texture* texture) {
    state_ptr->backend.destroy_texture(texture);
}

void renderer_set_view(mat4 view) {
    if (state_ptr) {
        state_ptr->view = view;
    }
}