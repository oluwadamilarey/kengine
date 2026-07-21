#include "linear_allocator.h"

#include "core/kmemory.h"
#include "core/logger.h"

void linear_allocator_create(u64 total_size, void* memory, linear_allocator* out_allocator) {
    if (out_allocator) {
        out_allocator->memory = memory;
        out_allocator->total_size = total_size;
        out_allocator->allocated = 0;
        out_allocator->owns_memory = memory == 0;

        if (memory) {
            out_allocator->memory = memory;
        } else {
            out_allocator->memory = kallocate(total_size, MEMORY_TAG_ARRAY);
        }
    }
}

void linear_allocator_initialize(linear_allocator* allocator, u64 total_size) {
    if (allocator) {
        allocator->total_size = total_size;
        allocator->allocated = 0;
        allocator->owns_memory = true;
        allocator->memory = kallocate(total_size, MEMORY_TAG_ARRAY);
    }
}

void linear_allocator_destroy(linear_allocator* allocator) {
    if (allocator) {
        allocator->allocated = 0;
        if (allocator->owns_memory && allocator->memory) {
            kfree(allocator->memory, allocator->total_size, MEMORY_TAG_ARRAY);
        } else {
            allocator->memory = 0;
        }
        allocator->total_size = 0;
        allocator->owns_memory = false;
    }
}

void* linear_allocator_allocate(linear_allocator* allocator, u64 size) {
    if (allocator && allocator->memory) {
        if (allocator->allocated + size <= allocator->total_size) {
            void* block = (u8*)allocator->memory + allocator->allocated;
            allocator->allocated += size;
            return block;
        } else {
            KERROR("Linear allocator out of memory. Requested: %llu bytes, Available: %llu bytes",
                   size,
                   allocator->total_size - allocator->allocated);
            return 0;
        }
    }
    return 0;
}

void linear_allocator_free_all(linear_allocator* allocator) {
    if (allocator) {
        allocator->allocated = 0;
    }
}