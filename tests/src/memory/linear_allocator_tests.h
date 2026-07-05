#pragma once
#include "defines.h"
#include "../expect.h"

void linear_allocator_register_tests();

void linear_allocator_register_tests();

u8 linear_allocator_multi_allocation_all_space_then_free();

u8 linear_allocator_multi_allocation_all_space();

u8 linear_allocator_single_allocation_all_space();

u8 linear_allocator_should_create_and_destroy();