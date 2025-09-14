#include "platform/platform.h"

// macOS platform layer using GLFW.
#if __APPLE__ && TARGET_OS_MAC

#include "core/logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <string.h>

typedef struct internal_state {
    GLFWwindow* window;

    // Event state
    b8 quit_flagged;

    // Timing
    mach_timebase_info_data_t timebase_info;
    u64 start_time;

    // Window properties
    i32 width;
    i32 height;

    // Input state tracking
    b8 keys[GLFW_KEY_LAST + 1];
    b8 mouse_buttons[GLFW_MOUSE_BUTTON_LAST + 1];
    f64 mouse_x;
    f64 mouse_y;
} internal_state;

// Forward declarations for callbacks
static void glfw_error_callback(int error, const char* description);
static void glfw_window_close_callback(GLFWwindow* window);
static void glfw_window_size_callback(GLFWwindow* window, int width, int height);
static void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height);
static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

b8 platform_startup(
    platform_state* plat_state,
    const char* application_name,
    i32 x,
    i32 y,
    i32 width,
    i32 height) {
    // Create the internal state
    plat_state->internal_state = malloc(sizeof(internal_state));
    internal_state* state = (internal_state*)plat_state->internal_state;
    memset(state, 0, sizeof(internal_state));

    // Store dimensions
    state->width = width;
    state->height = height;
    state->quit_flagged = FALSE;

    // Set GLFW error callback
    glfwSetErrorCallback(glfw_error_callback);

    // Initialize GLFW
    if (!glfwInit()) {
        KFATAL("Failed to initialize GLFW.");
        return FALSE;
    }

    // Configure GLFW for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window
    state->window = glfwCreateWindow(width, height, application_name, NULL, NULL);
    if (!state->window) {
        KFATAL("Failed to create GLFW window.");
        glfwTerminate();
        return FALSE;
    }

    // Set window position (GLFW doesn't support this in window creation)
    glfwSetWindowPos(state->window, x, y);

    // Store platform state in window user pointer for callbacks
    glfwSetWindowUserPointer(state->window, state);

    // Set callbacks
    glfwSetWindowCloseCallback(state->window, glfw_window_close_callback);
    glfwSetWindowSizeCallback(state->window, glfw_window_size_callback);
    glfwSetFramebufferSizeCallback(state->window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(state->window, glfw_key_callback);
    glfwSetMouseButtonCallback(state->window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(state->window, glfw_cursor_position_callback);
    glfwSetScrollCallback(state->window, glfw_scroll_callback);

    // Enable raw mouse motion if available (for FPS-style games)
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(state->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Setup high-precision timing
    mach_timebase_info(&state->timebase_info);
    state->start_time = mach_absolute_time();

    KINFO("macOS platform (GLFW) initialized successfully.");
    return TRUE;
}

void platform_shutdown(platform_state* plat_state) {
    internal_state* state = (internal_state*)plat_state->internal_state;

    if (state->window) {
        glfwDestroyWindow(state->window);
        state->window = NULL;
    }

    glfwTerminate();

    if (plat_state->internal_state) {
        free(plat_state->internal_state);
        plat_state->internal_state = NULL;
    }

    KINFO("macOS platform (GLFW) shutdown.");
}

b8 platform_pump_messages(platform_state* plat_state) {
    internal_state* state = (internal_state*)plat_state->internal_state;

    // Poll for and process events
    glfwPollEvents();

    // Check if window should close
    if (glfwWindowShouldClose(state->window)) {
        state->quit_flagged = TRUE;
    }

    return !state->quit_flagged;
}

void* platform_allocate(u64 size, b8 aligned) {
    if (aligned) {
        // Align to 16-byte boundary for SIMD operations
        void* ptr;
        if (posix_memalign(&ptr, 16, size) == 0) {
            return ptr;
        }
        return NULL;
    }
    return malloc(size);
}

void platform_free(void* block, b8 aligned) {
    if (block) {
        free(block);
    }
}

void* platform_zero_memory(void* block, u64 size) {
    return memset(block, 0, size);
}

void* platform_copy_memory(void* dest, const void* source, u64 size) {
    return memcpy(dest, source, size);
}

void* platform_set_memory(void* dest, i32 value, u64 size) {
    return memset(dest, value, size);
}

void platform_console_write(const char* message, u8 colour) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    const char* colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};
    printf("\033[%sm%s\033[0m", colour_strings[colour], message);
    fflush(stdout);
}

void platform_console_write_error(const char* message, u8 colour) {
    // FATAL,ERROR,WARN,INFO,DEBUG,TRACE
    const char* colour_strings[] = {"0;41", "1;31", "1;33", "1;32", "1;34", "1;30"};
    fprintf(stderr, "\033[%sm%s\033[0m", colour_strings[colour], message);
    fflush(stderr);
}

f64 platform_get_absolute_time() {
    // Use GLFW's high-resolution timer (more portable)
    return glfwGetTime();
}

// void platform_sleep(u64 ms) {
//     if (ms == 0) {
//         return;
//     }

//     struct timespec req;
//     req.tv_sec = ms / 1000;
//     req.tv_nsec = (ms % 1000) * 1000000;

//     nanosleep(&req, NULL);
// }

// Helper functions for graphics integration

void* platform_get_native_window_handle(platform_state* plat_state) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    return state->window;
}

void platform_get_window_size(platform_state* plat_state, i32* width, i32* height) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    *width = state->width;
    *height = state->height;
}

void platform_get_framebuffer_size(platform_state* plat_state, i32* width, i32* height) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    glfwGetFramebufferSize(state->window, width, height);
}

void platform_set_window_title(platform_state* plat_state, const char* title) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    glfwSetWindowTitle(state->window, title);
}

b8 platform_vulkan_surface_create(platform_state* plat_state, void* vulkan_instance, void* surface) {
    internal_state* state = (internal_state*)plat_state->internal_state;

    // GLFW handles Vulkan surface creation internally
    VkResult result = glfwCreateWindowSurface((VkInstance)vulkan_instance,
                                              state->window,
                                              NULL,
                                              (VkSurfaceKHR*)surface);

    return result == VK_SUCCESS;
}

const char** platform_get_required_vulkan_extensions(u32* count) {
    return glfwGetRequiredInstanceExtensions(count);
}

// Input query functions

b8 platform_is_key_pressed(platform_state* plat_state, i32 key) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    if (key >= 0 && key <= GLFW_KEY_LAST) {
        return state->keys[key];
    }
    return FALSE;
}

b8 platform_is_mouse_button_pressed(platform_state* plat_state, i32 button) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST) {
        return state->mouse_buttons[button];
    }
    return FALSE;
}

void platform_get_mouse_position(platform_state* plat_state, f64* x, f64* y) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    *x = state->mouse_x;
    *y = state->mouse_y;
}

void platform_set_mouse_position(platform_state* plat_state, f64 x, f64 y) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    glfwSetCursorPos(state->window, x, y);
}

void platform_show_cursor(platform_state* plat_state, b8 show) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    glfwSetInputMode(state->window, GLFW_CURSOR,
                     show ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

void platform_lock_cursor(platform_state* plat_state, b8 lock) {
    internal_state* state = (internal_state*)plat_state->internal_state;
    glfwSetInputMode(state->window, GLFW_CURSOR,
                     lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

// GLFW Callback implementations

static void glfw_error_callback(int error, const char* description) {
    KERROR("GLFW Error (%d): %s", error, description);
}

static void glfw_window_close_callback(GLFWwindow* window) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state) {
        state->quit_flagged = TRUE;
    }
}

static void glfw_window_size_callback(GLFWwindow* window, int width, int height) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state) {
        state->width = width;
        state->height = height;
        // TODO: Fire window resize event
        KDEBUG("Window resized: %dx%d", width, height);
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state) {
        // TODO: Fire framebuffer resize event (important for Vulkan swapchain)
        KDEBUG("Framebuffer resized: %dx%d", width, height);
    }
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state && key >= 0 && key <= GLFW_KEY_LAST) {
        b8 pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);
        state->keys[key] = pressed;

        // TODO: Fire key event
        KDEBUG("Key %s: %d (scancode: %d, mods: %d)",
               pressed ? "pressed" : "released", key, scancode, mods);
    }
}

static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state && button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST) {
        b8 pressed = (action == GLFW_PRESS);
        state->mouse_buttons[button] = pressed;

        // TODO: Fire mouse button event
        KDEBUG("Mouse button %s: %d (mods: %d)",
               pressed ? "pressed" : "released", button, mods);
    }
}

static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state) {
        state->mouse_x = xpos;
        state->mouse_y = ypos;

        // TODO: Fire mouse move event
        // Note: Only log this in verbose debug mode as it's very frequent
    }
}

static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    internal_state* state = (internal_state*)glfwGetWindowUserPointer(window);
    if (state) {
        // TODO: Fire scroll event
        KDEBUG("Mouse scroll: %f, %f", xoffset, yoffset);
    }
}

#endif  // KPLATFORM_APPLE && TARGET_OS_MAC