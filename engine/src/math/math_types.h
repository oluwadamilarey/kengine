#pragma once
#include "defines.h"

typedef struct vec2 {
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
        union {
            // the third element, for padding to 16 bytes (128 bits) for SIMD alignment. Not used for storage, but can be used for calculations if desired.
            f32 z, b, p, w;
        };
    };
} vec3;

typedef struct vec4_u {
#if defined(KUSE_SIMD)
    // used for SIMD operations, but not for storage. The actual data is stored in the elements array.
    alignas(16) __m128 data;
#endif
    // An array of x, y, z, w components. Used for storage and non-SIMD operations.
    alignas(16) f32 elements[4];
    union {
        struct
        {
            union {
                // the first element
                f32 x, r, s;
            };
            union {
                // the second element
                f32 y, g, t;
            };
            union {
                // the third element
                f32 z, b, p;
            };
            union {
                // the fourth element
                f32 w, a, q;
            };
        };
    };
} vec4;

typedef vec4 quat;

typedef union mat4_u {
    f32 data[16];
} mat4;