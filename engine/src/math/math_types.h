#pragma once
#include "defines.h"

typedef union vec2_u {
    f32 elements[2];
    struct {
        union {
            // first element
            f32 x, r, s, u;
        };
        union {
            // second element
            f32 y, g, t, v;
        };
    };
} vec2;

typedef union vec3_u {
    f32 elements[3];
    struct {
        union {
            // first element
            f32 x, r, s, u;
        };
        union {
            // second element
            f32 y, g, t, v;
        };
        union {
            // third element
            f32 z, b, p, w;
        };
    };
} vec3;

typedef union vec4_u {
#if defined(KUSE_SIMD)
    alignas(16) __m128 data;
#endif
    alignas(16) f32 elements[4];
    struct {
        union {
            f32 x, r, s;
        };
        union {
            f32 y, g, t;
        };
        union {
            f32 z, b, p;
        };
        union {
            f32 w, a, q;
        };
    };
} vec4;

typedef vec4 quat;

typedef union mat4_u {
    f32 data[16];
} mat4;