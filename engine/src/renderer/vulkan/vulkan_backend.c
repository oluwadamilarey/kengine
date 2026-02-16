#include "vulkan_backend.h"

#include "vulkan_types.inl"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_command_buffer.h"
#include "vulkan_image.h"

#include "core/logger.h"
#include "core/kstring.h"

#include "containers/darray.h"
#include "platform/platform.h"
#include "vulkan_platform.h"
#include "core/kstring.h"

// static Vulkan context
static vulkan_context context;

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

i32 find_memory_index(u32 type_filter, u32 property_flags);

void create_command_buffers(renderer_backend* backend);

/**
 * @brief Initializes the Vulkan renderer backend.
 *
 * This function sets up the entire Vulkan rendering infrastructure in the following order:
 * 1. VkInstance creation (the connection between the application and Vulkan)
 * 2. Debug messenger setup (for validation layer messages in debug builds)
 * 3. Surface creation (platform-specific window surface for rendering)
 * 4. Logical device creation (interface to the physical GPU)
 * 5. Swapchain creation (manages the images we render to and present)
 * 6. Renderpass creation (defines how rendering operations are structured)
 * 7. Command buffer allocation (stores GPU commands for execution)
 *
 * @param backend Pointer to the renderer backend structure to initialize.
 * @param application_name The name of the application (shown in debugging tools).
 * @param plat_state Pointer to platform-specific state needed for surface creation.
 * @return TRUE if initialization succeeds, FALSE otherwise.
 */
b8 vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, struct platform_state* plat_state) {
    /*
     * ========================================================================
     * CONTEXT INITIALIZATION
     * ========================================================================
     * The vulkan_context is a static global that holds all Vulkan state.
     * We start by setting up utility function pointers and the allocator.
     */

    // Store a function pointer for finding suitable memory types during buffer/image allocation.
    // This is used throughout the renderer when allocating GPU memory (e.g., vertex buffers, textures).
    context.find_memory_index = find_memory_index;

    // TODO: Implement a custom Vulkan memory allocator for better memory management.
    // Using NULL (0) tells Vulkan to use its internal default allocator.
    // A custom allocator would give us more control over memory allocation patterns.
    context.allocator = 0;

    /*
     * ========================================================================
     * VULKAN INSTANCE CREATION
     * ========================================================================
     * The VkInstance is the connection between our application and the Vulkan library.
     * It holds per-application state and allows us to enumerate physical devices.
     */

    // VkApplicationInfo provides information about our application to the Vulkan driver.
    // Some drivers may use this info for optimizations specific to well-known engines.
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};  // sType must be set for all Vulkan structs
    app_info.apiVersion = VK_API_VERSION_1_2;       // Request Vulkan 1.2 API (provides modern features like timeline semaphores)
    app_info.pApplicationName = application_name;   // Application name (visible in GPU profiling tools)
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);  // Application version (major.minor.patch)
    app_info.pEngineName = "Kohi Engine";           // Engine name (also visible in profiling tools)
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);  // Engine version

    // VkInstanceCreateInfo describes how to create the VkInstance.
    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    /*
     * ------------------------------------------------------------------------
     * INSTANCE EXTENSIONS
     * ------------------------------------------------------------------------
     * Extensions provide functionality beyond the core Vulkan specification.
     * We need platform-specific extensions for window surface creation.
     */

    // Create a dynamic array to hold required extension names.
    // Using darray allows us to build the list dynamically based on platform/debug settings.
    const char** required_extensions = darray_create(const char*);

    // VK_KHR_surface: Core extension for presenting rendered images to a window.
    // This is required on ALL platforms that want to display graphics.
    darray_push(required_extensions, &VK_KHR_SURFACE_EXTENSION_NAME);

    /*
     * Platform-specific surface extensions:
     * Each platform has its own way of creating window surfaces.
     * - macOS: Uses Metal via MoltenVK translation layer
     * - Linux: Uses XCB (X11 protocol library) for X Window System
     * - Windows: Uses Win32 API
     */
#if defined(KPLATFORM_APPLE)
    // VK_EXT_metal_surface: Creates a Vulkan surface from a CAMetalLayer.
    // MoltenVK translates Vulkan calls to Metal on Apple platforms.
    darray_push(required_extensions, &VK_EXT_METAL_SURFACE_EXTENSION_NAME);

    // VK_KHR_portability_enumeration: Required on macOS to enumerate devices
    // that don't fully conform to Vulkan spec (MoltenVK is a "portability" implementation).
    darray_push(required_extensions, &VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#elif defined(KPLATFORM_LINUX)
    // VK_KHR_xcb_surface: Creates surfaces for X11 windows via XCB.
    darray_push(required_extensions, &VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(KPLATFORM_WINDOWS)
    // VK_KHR_win32_surface: Creates surfaces for Win32 windows (HWND).
    darray_push(required_extensions, &VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

#if defined(_DEBUG)
    // VK_EXT_debug_utils: Enables debug callbacks for validation layer messages.
    // This allows us to receive warnings/errors about incorrect Vulkan usage.
    darray_push(required_extensions, &VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Log all requested extensions for debugging purposes.
    KDEBUG("Requested Vulkan extensions:");
    u64 req_ext_count = darray_length(required_extensions);
    for (u64 i = 0; i < req_ext_count; ++i) {
        KDEBUG("  %s", required_extensions[i]);
    }
#endif
    // Allow the platform layer to add any additional required extensions.
    // This provides flexibility for platform-specific requirements we might not know about here.
    platform_get_required_extension_names(&required_extensions);

    // Finalize extension configuration for instance creation.
    u32 extension_count = (u32)darray_length(required_extensions);
    const char** extensions = required_extensions;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = (const char* const*)extensions;

    /*
     * ------------------------------------------------------------------------
     * VALIDATION LAYERS (Debug builds only)
     * ------------------------------------------------------------------------
     * Validation layers intercept Vulkan calls and check for incorrect usage.
     * They're essential during development but have performance overhead.
     */

    const char** required_validation_layer_names = 0;
    u32 required_validation_layer_count = 0;

#if defined(_DEBUG)
    KINFO("Validation layers enabled. Enumerating...");

    // Create a list of validation layers we want to enable.
    required_validation_layer_names = darray_create(const char*);

    // VK_LAYER_KHRONOS_validation: The standard, all-in-one validation layer.
    // It combines all individual validation checks (parameter, object lifetime, threading, etc.)
    const char* khronos_validation = "VK_LAYER_KHRONOS_validation";
    darray_push(required_validation_layer_names, khronos_validation);
    required_validation_layer_count = darray_length(required_validation_layer_names);

    /*
     * We must verify that requested validation layers are actually available.
     * Not all systems have the Vulkan SDK installed with validation layers.
     */

    // First call: Get the count of available layers (pass NULL for the array).
    u32 available_layer_count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));

    // Second call: Retrieve the actual layer properties.
    VkLayerProperties* available_layers = darray_reserve(VkLayerProperties, available_layer_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

    // Check each required layer against the list of available layers.
    // If any required layer is missing, we cannot proceed (validation is mandatory in debug).
    for (u32 i = 0; i < required_validation_layer_count; ++i) {
        KINFO("Searching for layer: %s...", required_validation_layer_names[i]);
        b8 found = FALSE;
        for (u32 j = 0; j < available_layer_count; ++j) {
            if (strings_equal(required_validation_layer_names[i], available_layers[j].layerName)) {
                found = TRUE;
                KINFO("Found.");
                break;
            }
        }
        if (!found) {
            // Fatal error: Cannot run debug build without validation.
            // User needs to install the Vulkan SDK.
            KFATAL("Required validation layer is missing: %s", required_validation_layer_names[i]);
            return FALSE;
        }
    }
    KINFO("All required validation layers are present.");
#endif

    // Configure validation layers in the instance create info.
    // In release builds, these will be 0/NULL (no validation overhead).
    create_info.enabledLayerCount = required_validation_layer_count;
    create_info.ppEnabledLayerNames = (const char* const*)required_validation_layer_names;

#if defined(KPLATFORM_APPLE)
    // Required flag for MoltenVK: Allows enumeration of "non-conformant" Vulkan implementations.
    // MoltenVK doesn't fully implement Vulkan (it translates to Metal), so this flag is needed.
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    /*
     * Create the VkInstance!
     * This is the first major Vulkan object we create.
     * All subsequent Vulkan calls will use this instance.
     */
    VK_CHECK(vkCreateInstance(&create_info, context.allocator, &context.instance));

    /*
     * ========================================================================
     * DEBUG MESSENGER SETUP (Debug builds only)
     * ========================================================================
     * The debug messenger receives callbacks from validation layers.
     * Messages include errors, warnings, and performance suggestions.
     */
#if defined(_DEBUG)
    KDEBUG("Creating Vulkan debugger...");

    // Configure which message severities we want to receive.
    // VERBOSE is commented out as it produces too much output for normal debugging.
    u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |    // API misuse, crashes
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |  // Potential issues
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;      // Informational
                       // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT; // Diagnostic spam

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;

    // Configure which types of messages we want:
    // - GENERAL: Non-specification events (e.g., creation/destruction)
    // - PERFORMANCE: Potential performance issues (e.g., suboptimal resource usage)
    // - VALIDATION: Specification violations (e.g., invalid parameters, incorrect state)
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

    // Our callback function that will receive all debug messages.
    debug_create_info.pfnUserCallback = vk_debug_callback;

    /*
     * vkCreateDebugUtilsMessengerEXT is an extension function, not part of core Vulkan.
     * We must look up its address using vkGetInstanceProcAddr.
     * This pattern is common for extension functions.
     */
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");
    KASSERT_MSG(func, "Failed to create debug messenger!");

    // Create the debug messenger.
    VK_CHECK(func(context.instance, &debug_create_info, context.allocator, &context.debug_messenger));
    KDEBUG("Vulkan debugger created.");
#endif

    /*
     * ========================================================================
     * SURFACE CREATION
     * ========================================================================
     * A VkSurfaceKHR represents a platform-specific window surface.
     * It's the bridge between Vulkan and the windowing system.
     * The swapchain will use this surface to present rendered images.
     */
    if (!platform_create_vulkan_surface(plat_state, &context)) {
        KFATAL("Failed to create Vulkan surface.");
        return FALSE;
    }

    /*
     * ========================================================================
     * LOGICAL DEVICE CREATION
     * ========================================================================
     * The logical device (VkDevice) is our interface to a physical GPU.
     * vulkan_device_create() handles:
     * - Enumerating and selecting a suitable physical device (GPU)
     * - Creating queues for graphics, compute, and transfer operations
     * - Enabling required device extensions (e.g., VK_KHR_swapchain)
     */
    if (!vulkan_device_create(&context)) {
        KFATAL("Failed to create Vulkan device.");
        return FALSE;
    }

    /*
     * ========================================================================
     * SWAPCHAIN CREATION
     * ========================================================================
     * The swapchain manages a set of images that we render to and present.
     * It handles double/triple buffering and synchronization with the display.
     * Parameters: context, width, height, output swapchain.
     */
    vulkan_swapchain_create(&context, context.framebuffer_width, context.framebuffer_height, &context.swapchain);

    /*
     * ========================================================================
     * RENDERPASS CREATION
     * ========================================================================
     * A renderpass defines the structure of rendering operations:
     * - What attachments (color, depth) are used
     * - How they're loaded/stored (clear, preserve, don't care)
     * - Subpass dependencies for synchronization
     *
     * Parameters explained:
     * - context: Vulkan context
     * - main_renderpass: Output renderpass handle
     * - x, y: Render area origin (0, 0 = full framebuffer)
     * - width, height: Render area dimensions
     * - r, g, b, a: Clear color (0, 0, 0, 1 = opaque black)
     * - depth: Clear depth value (1.0 = far plane)
     * - stencil: Clear stencil value (0)
     */
    vulkan_renderpass_create(&context, &context.main_renderpass, 0.0f, 0.0f,
                             (f32)context.framebuffer_width, (f32)context.framebuffer_height,
                             0.0f, 0.0f, 0.0f, 1.0f,  // Clear to opaque black
                             1.0f, 0);                 // Depth=1.0, stencil=0

    /*
     * ========================================================================
     * COMMAND BUFFER CREATION
     * ========================================================================
     * Command buffers record GPU commands (draw calls, dispatches, etc.)
     * We create one command buffer per swapchain image to allow recording
     * commands for the next frame while the current frame is being presented.
     */
    create_command_buffers(backend);

    KINFO("Vulkan renderer initialized successfully.");
    return TRUE;
}

void vulkan_renderer_backend_shutdown(renderer_backend* backend) {
    // renderpass destroy
    vulkan_renderpass_destroy(&context, &context.main_renderpass);
    // destroy in the opposite order of creation
    vulkan_swapchain_destroy(&context, &context.swapchain);
    vulkan_device_destroy(&context);
    if (context.surface) {
        vkDestroySurfaceKHR(context.instance, context.surface, context.allocator);
        context.surface = VK_NULL_HANDLE;
    }
    KDEBUG("Shutting down Vulkan renderer backend...");
    if (context.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT func =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
        func(context.instance, context.debug_messenger, context.allocator);
    }
    KDEBUG("Destroying Vulkan instance...");
    vkDestroyInstance(context.instance, context.allocator);
    KDEBUG("Vulkan instance destroyed.");
    KDEBUG("Vulkan renderer backend shut down.");
}
void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height) {
    context.framebuffer_width = width;
    context.framebuffer_height = height;
    context.recreating_swapchain = TRUE;
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time) {
    return TRUE;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time) {
    return TRUE;
}

/**
 * @brief Vulkan debug callback function.
 *
 * @param message_severity
 * @param message_types
 * @param callback_data
 * @param user_data
 * @return VkBool32
 */
VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            KERROR(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            KWARN(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            KINFO(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            KINFO(callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

/**
 * @brief Finds a memory type index that satisfies the given type filter and property flags.
 *
 * @param type_filter
 * @param property_flags
 * @return i32
 */
i32 find_memory_index(u32 type_filter, u32 property_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context.device.physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
        // check each memory type to see if it bits are set to 1, meaning that type is supported
        if ((type_filter & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags) {
            return (i);
        }
    }
    return -1;
}

// void create_command_buffers(renderer_backend* backend) {
//     // Command buffer creation is deferred until after the swapchain is created, since command buffers reference the renderpass which references the swapchain's image format.
//     // This allows us to avoid having to recreate command buffers when the swapchain is recreated on resize.
//     vulkan_command_buffer_create(&context, &context.main_renderpass, &context.swapchain);
// }

// void create_command_buffers(renderer_backend* backend) {
//     if(!context.graphics_command_buffers) {
//         context.graphics_command_buffers = darray_reserve(vulkan_command_buffer, context.swapchain.image_count);
//         for(u32 i = 0; i < context.swapchain.image_count; ++i) {
//             vulkan_command_buffer cmd_buffer;
//             vulkan_command_buffer_create(&context, &cmd_buffer);
//             darray_push(context.graphics_command_buffers, cmd_buffer);
//         }
//     }
// }

void create_command_buffers(renderer_backend* backend) {
    if (!context.graphics_command_buffers) {
        context.graphics_command_buffers = darray_reserve(vulkan_command_buffer, context.swapchain.image_count);
        for (u32 i = 0; i < context.swapchain.image_count; ++i) {
            kzero_memory(&context.graphics_command_buffers[i], sizeof(vulkan_command_buffer));
            // vulkan_command_buffer_create(&context, &context.graphics_command_buffers[i]);
        }
    }

    for (u32 i = 0; i < context.swapchain.image_count; ++i) {
        if (context.graphics_command_buffers[i].handle) {
            vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);
        }
        // 
        kzero_memory(&context.graphics_command_buffers[i], sizeof(vulkan_command_buffer));
        vulkan_command_buffer_allocate(&context, context.device.graphics_command_pool, TRUE, &context.graphics_command_buffers[i]);
    }
}