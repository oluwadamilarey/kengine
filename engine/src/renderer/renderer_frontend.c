#include "renderer_frontend.h"
#include "renderer_backend.h"

#include "core/logger.h"
#include "core/kmemory.h"
#include "math/kmath.h"

#include "core/kstring.h"
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
    texture cobblestone_texture;
    texture test_diffuse;
} renderer_system_state;

// Backend render context. Points into memory owned by application_state's
// linear allocator — this module never allocates or frees it itself.
static renderer_system_state* state_ptr;


b8 event_on_debug_event(u16 code, void* sender, void* listener_inst, event_context data) {
    const char* names[3] = {
        "cobblestone",
        "paving",
        "paving2"};
    static i8 choice = 2;
    choice++;
    choice %= 3;

    // load up the new texture
    load_texture(names[choice], &state_ptr->test_diffuse);
    return true;
}

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name, struct platform_state* plat_state) {
    *memory_requirement = sizeof(renderer_system_state);
    if (state == 0) {
        return true;
    }

    state_ptr = state;

    event_register(EVENT_CODE_DEBUG0, state_ptr, event_on_debug_event);

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

    
    // TODO: load other textures here, or provide a way for the application to load them.
    create_texture(&state_ptr->test_diffuse);
    if (!load_texture("paving", &state_ptr->test_diffuse)) {
        KWARN("Failed to load cobblestone texture, falling back to default.");
    }

    return true;
}

void renderer_system_shutdown() {
    if (state_ptr) {
        event_unregister(EVENT_CODE_DEBUG0, state_ptr, event_on_debug_event);
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
        data.textures[0] = &state_ptr->test_diffuse;  // use the test diffuse texture for the object
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
                             i32 width,
                             i32 height,
                             i32 channel_count,
                             b8 has_transparency,
                             const u8* pixels,
                             struct texture* out_texture) {
    state_ptr->backend.create_texture(name, width, height, channel_count, has_transparency, pixels, out_texture);
};

void renderer_destroy_texture(struct texture* texture) {
    state_ptr->backend.destroy_texture(texture);
}

void renderer_set_view(mat4 view) {
    if (state_ptr) {
        state_ptr->view = view;
    }
}