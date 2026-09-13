#pragma once

#include "renderer_types.inl"

struct static_mesh_data;
struct platform_state;

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name, struct platform_state* plat_state);
void renderer_system_shutdown();

void renderer_on_resized(u16 width, u16 height);

b8 renderer_draw_frame(render_packet* packet);

// HACK: This should not be exposed outside the engine
KAPI void renderer_set_view(mat4 view);
