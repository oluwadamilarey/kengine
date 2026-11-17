#include "test_manager.h"

#include "memory/linear_allocator_tests.h"
#include "containers/hashtable_tests.h"

#include <core/logger.h>
#include <core/kmemory.h> // TODO: Memory system tests.

int main() {
    // Initialize the memory system first so kallocate/kfree calls are tracked.
    u64 memory_requirement = 0;
    initialize_memory(&memory_requirement, 0);
    void* memory_system_state = kallocate(memory_requirement, MEMORY_TAG_APPLICATION);
    initialize_memory(&memory_requirement, memory_system_state);

    // Always initalize the test manager next.
    test_manager_init();

    // TODO: add test registrations here.
    linear_allocator_register_tests();
    hashtable_register_tests();

    KDEBUG("Starting tests...");

    // Execute tests
    test_manager_run_tests();

    // Shut down the memory system last, after everything using it is done.
    kfree(memory_system_state, memory_requirement, MEMORY_TAG_APPLICATION);
    shutdown_memory(memory_system_state);

    return 0;
}