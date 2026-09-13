#include "renderer_frontend.h"
#include "renderer_backend.h"

#include "core/logger.h"
#include "core/kmemory.h"
#include "math/kmath.h"

typedef struct renderer_system_state {
    renderer_backend backend;
    mat4 projection;
    mat4 view;
    f32 near_clip;
    f32 far_clip;
} renderer_system_state;

// Backend render context. Points into memory owned by application_state's
// linear allocator — this module never allocates or frees it itself.
static renderer_system_state* state_ptr;

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name, struct platform_state* plat_state) {
    *memory_requirement = sizeof(renderer_system_state);
    if (state == 0) {
        // First pass: caller only wants the size, so it can allocate the block.
        return true;
    }

    state_ptr = state;

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

    return true;
}

void renderer_system_shutdown() {
    if (state_ptr) {
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
        state_ptr->backend.update_object(model);

        b8 result = renderer_end_frame(packet->delta_time);
        if (!result) {
            KERROR("renderer_end_frame failed. Application shutting down...");
            return false;
        }
    }

    return true;
}

void renderer_set_view(mat4 view) {
    if (state_ptr) {
        state_ptr->view = view;
    }
}