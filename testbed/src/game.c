#include "game.h"

#include <core/logger.h>
#include <core/kmemory.h>
#include <core/input.h>
#include <math/kmath.h>
// HACK: This should not be available outside the engine
#include <renderer/renderer_frontend.h>

void recalculate_view_matrix(game_state* state) {
    if (state->camera_view_dirty) {
        // mat4 rotation_x = mat4_rotation_x(state->camera_euler.x);
        // mat4 rotation_y = mat4_rotation_y(state->camera_euler.y);
        // mat4 rotation_z = mat4_rotation_z(state->camera_euler.z);

        // mat4 rotation_matrix = mat4_multiply(rotation_z, mat4_multiply(rotation_y, rotation_x));
        // mat4 translation_matrix = mat4_translation(state->camera_position);

        // state->view = mat4_inverse(mat4_multiply(translation_matrix, rotation_matrix));
        mat4 rotation = mat4_euler_xyz(state->camera_euler.x, state->camera_euler.y, state->camera_euler.z);
        mat4 translation = mat4_translation(state->camera_position);

        state->view = mat4_mul(rotation, translation);
        state->view = mat4_inverse(state->view);
        state->camera_view_dirty = false;
    }
}

void camera_yaw(game_state* state, f32 amount) {
    state->camera_euler.y += amount;
    state->camera_view_dirty = true;
}

void camera_pitch(game_state* state, f32 amount) {
    state->camera_euler.x += amount;
    state->camera_view_dirty = true;
}

b8 game_initialize(game* game_inst) {
    KDEBUG("game_initialize() called!");

    game_state* state = (game_state*)game_inst->state;

    state->camera_euler = (vec3){0, 0, 0};
    state->camera_position = vec3_zero();
    state->camera_view_dirty = true;

    f32 z = 0.001f;
    state->view = mat4_translation((vec3){0, 0, z});
    state->view = mat4_inverse(state->view);
    state->camera_view_dirty = true;

    return true;
}

b8 game_update(game* game_inst, f32 delta_time) {
    static u64 alloc_count = 0;
    u64 prev_alloc_count = alloc_count;
    alloc_count = get_memory_alloc_count();
    if (input_is_key_up('M') && input_was_key_down('M')) {
        KDEBUG("Allocations: %llu (%llu this frame)", alloc_count, alloc_count - prev_alloc_count);
    }

    recalculate_view_matrix((game_state*)game_inst->state);


    
    renderer_set_view(((game_state*)game_inst->state)->view);
    return true;
}

b8 game_render(game* game_inst, f32 delta_time) {
    return true;
}

void game_on_resize(game* game_inst, u32 width, u32 height) {
}