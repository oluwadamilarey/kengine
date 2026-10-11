#include "game.h"

#include <core/logger.h>
#include <core/kmemory.h>
#include <core/input.h>
#include <core/event.h>
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

void camera_orbit(game_state* state, vec3 target, f32 radius, f32 angle_rad) {
    state->camera_position.x = target.x + radius * ksin(angle_rad);
    state->camera_position.z = target.z + radius * kcos(angle_rad);
    state->camera_position.y = target.y;

    // face the target
    vec3 dir = vec3_sub(target, state->camera_position);
    state->camera_euler.y = katan2(dir.x, dir.z);  // adjust sign/axis convention to your kmath
    state->camera_view_dirty = true;
}

void camera_orbit_mouse(game_state* state, f32 dx, f32 dy) {
    f32 sensitivity = 0.005f;

    state->camera_euler.y -= dx * sensitivity;
    state->camera_euler.x -= dy * sensitivity;

    f32 yaw = state->camera_euler.y;
    f32 pitch = state->camera_euler.x;

    state->camera_position.x = state->orbit_target.x + state->orbit_radius * kcos(pitch) * ksin(yaw);
    state->camera_position.y = state->orbit_target.y + state->orbit_radius * ksin(pitch);
    state->camera_position.z = state->orbit_target.z + state->orbit_radius * kcos(pitch) * kcos(yaw);

    state->camera_view_dirty = true;
}

void camera_roll(game_state* state, f32 amount) {
    state->camera_euler.z += amount;
    state->camera_view_dirty = true;
}

void camera_zoom_dolly(game_state* state, f32 amount) {
    // move along the view's forward axis
    mat4 view = state->view;  // NOTE: use the *previous* frame's view, or forward from euler directly
    vec3 forward = mat4_forward(view);
    state->camera_position = vec3_add(state->camera_position, vec3_mul_scalar(forward, amount));
    state->camera_view_dirty = true;
}

void camera_look_from_side(game_state* state) {
    state->camera_euler.y = deg_to_rad(90.0f);  // rotate the *camera's* orientation
    state->camera_view_dirty = true;
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

    state->camera_euler = vec3_zero();
    state->camera_position = (vec3){0, 0, 30.0f};
    state->camera_view_dirty = true;

    state->orbit_target = vec3_zero();  // or the quad's world position
    state->orbit_radius = 30.0f;

    state->view = mat4_translation(state->camera_position);
    state->view = mat4_inverse(state->view);

    return true;
}

b8 game_update(game* game_inst, f32 delta_time) {
    static u64 alloc_count = 0;
    u64 prev_alloc_count = alloc_count;
    alloc_count = get_memory_alloc_count();
    if (input_is_key_up('M') && input_was_key_down('M')) {
        KDEBUG("Allocations: %llu (%llu this frame)", alloc_count, alloc_count - prev_alloc_count);
    }

    if (input_is_key_up('T') && input_was_key_down('T')) {
        KDEBUG("swapping texture");
        event_context context = {};
        event_fire(EVENT_CODE_DEBUG0, game_inst, context);
    }

    game_state* state = (game_state*)game_inst->state;

    // HACK: Temp Hack to move the camera around
    if (input_is_key_down('A') || input_is_key_down(KEY_LEFT)) {
        camera_yaw(state, deg_to_rad(90.0f) * delta_time);  // ~90°/sec, adjust to taste
    }
    if (input_is_key_down('D') || input_is_key_down(KEY_RIGHT)) {
        camera_yaw(state, -deg_to_rad(90.0f) * delta_time);
    }
    if (input_is_key_down('W') || input_is_key_down(KEY_UP)) {
        camera_pitch(state, deg_to_rad(90.0f) * delta_time);
    }
    if (input_is_key_down('S') || input_is_key_down(KEY_DOWN)) {
        camera_pitch(state, -deg_to_rad(90.0f) * delta_time);
    }

    f32 temp_move_speed = 50.0f;
    vec3 velocity = vec3_zero();

    if (input_is_key_down('W')) {
        vec3 forward = mat4_forward(state->view);
        velocity = vec3_add(velocity, forward);
    }

    if (input_is_key_down('S')) {
        vec3 backward = mat4_backward(state->view);
        velocity = vec3_add(velocity, backward);
    }

    if (input_is_key_down('Q')) {
        vec3 forward = mat4_left(state->view);
        velocity = vec3_add(velocity, forward);
    }

    if (input_is_key_down('E')) {
        vec3 right = mat4_right(state->view);
        velocity = vec3_add(velocity, right);
    }

    if (input_is_key_down('V')) {
        velocity.y += 1.0f;
    }

    if (input_is_key_down('X')) {
        velocity.y -= 1.0f;
    }

    if (input_is_key_down('S')) {
        vec3 backward = mat4_backward(state->view);
        velocity = vec3_add(velocity, backward);
    }

    if (input_is_key_down('R')) {
        camera_roll(state, deg_to_rad(90.0f) * delta_time);
    }
    if (input_is_key_down('Z')) {
        camera_zoom_dolly(state, temp_move_speed * delta_time);
    }
    if (input_is_key_down('C')) {
        camera_zoom_dolly(state, -temp_move_speed * delta_time);
    }

    // snap to axis-aligned views, Blender-style
    // NOTE: KEY_NUMPAD1/3/7 only fire from a real numpad (via translate_keycode
    // in platform_macos.m) — a MacBook Air has none, so these are dead on that
    // hardware. Left in for when an external keyboard is plugged in.
    if (input_was_key_down(KEY_NUMPAD1) && input_is_key_up(KEY_NUMPAD1)) {
        // front view: looking down -Z
        state->camera_euler = vec3_zero();
        state->camera_view_dirty = true;
    }
    if (input_was_key_down(KEY_NUMPAD3) && input_is_key_up(KEY_NUMPAD3)) {
        // side view: rotate 90° around Y to see the quad edge-on
        state->camera_euler.y = deg_to_rad(90.0f);
        state->camera_view_dirty = true;
    }
    if (input_was_key_down(KEY_NUMPAD7) && input_is_key_up(KEY_NUMPAD7)) {
        // top view: pitch 90° down
        state->camera_euler.x = deg_to_rad(90.0f);
        state->camera_euler.y = 0.0f;
        state->camera_view_dirty = true;
    }

    // [ and ] for incremental orbit around Y, free keys not already claimed by movement
    if (input_is_key_down(KEY_LBRACKET)) {
        camera_yaw(state, deg_to_rad(90.0f) * delta_time);
    }
    if (input_is_key_down(KEY_RBRACKET)) {
        camera_yaw(state, -deg_to_rad(90.0f) * delta_time);
    }
    vec3 z = vec3_zero();
    if (!vec3_compare(z, velocity, 0.00002f)) {
        vec3_normalize(&velocity);
        state->camera_position.x += velocity.x * temp_move_speed * delta_time;
        state->camera_position.y += velocity.y * temp_move_speed * delta_time;
        state->camera_position.z += velocity.z * temp_move_speed * delta_time;
        state->camera_view_dirty = true;
    }

    // Trackpad orbit: Option (KEY_LALT) + left-click drag.
    // A MacBook trackpad has no physical middle button, so BUTTON_MIDDLE
    // never fires from it — platform_macos.m only sends BUTTON_MIDDLE from
    // otherMouseDown/Up (buttonNumber == 2), which a trackpad can't generate.
    // Option+left-click drag is the standard substitute (same idea as
    // Blender's "Emulate 3 Button Mouse"), and both KEY_LALT (flagsChanged:)
    // and BUTTON_LEFT (mouseDown:/mouseDragged:) are already wired for it.
    b8 orbit_modifier_down = input_is_key_down(KEY_LALT);
    b8 orbit_modifier_was_down = input_was_key_down(KEY_LALT);
    b8 orbit_click_down = input_is_button_down(BUTTON_LEFT);
    b8 orbit_click_was_down = input_was_button_down(BUTTON_LEFT);

    if (orbit_modifier_down && orbit_click_down) {
        i32 curr_x, curr_y, prev_x, prev_y;
        input_get_mouse_position(&curr_x, &curr_y);
        input_get_previous_mouse_position(&prev_x, &prev_y);

        // Only apply a delta if the modifier AND the click were both already
        // held last frame too — otherwise the very first frame either one
        // goes down still computes a delta against a stale/unrelated
        // previous mouse position, causing a jump instead of a smooth drag.
        if (orbit_modifier_was_down && orbit_click_was_down) {
            i32 dx = curr_x - prev_x;
            i32 dy = curr_y - prev_y;
            camera_orbit_mouse(state, (f32)dx, (f32)dy);
        }
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