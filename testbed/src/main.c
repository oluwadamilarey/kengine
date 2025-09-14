#include <core/logger.h>
#include <core/asserts.h>
#include <platform/platform.h>
#include <defines.h>
#include <stdbool.h>

int main() {
    KFATAL("A Test message: %f", 3.14f);
    KERROR("A Test message: %f", 3.14f);
    KWARN("A Test message: %f", 3.14f);
    KINFO("A Test message: %f", 3.14f);
    KDEBUG("A Test message: %f", 3.14f);

    platform_state state;

    if (platform_startup(&state, "Testbed", 100, 100, 1280, 720)) {
        // Platform started successfully, run the message loop
        while (platform_pump_messages(&state)) {
            // Keep running until the window is closed
            // platform_pump_messages returns false when window should close
        }
    } else {
        KFATAL("Failed to initialize platform!");
        return -1;
    }

    platform_shutdown(&state);
    return 0;
}