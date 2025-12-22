#include "vulkan_backend.h"

#include "vulkan_types.inl"

#include "core/logger.h"
#include "core/kstring.h"

#include "containers/darray.h"
#include "platform/platform.h"
#include "vulkan_platform.h"

// static Vulkan context
static vulkan_context context;

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, struct platform_state* plat_state) {
    // TODO: custom allocator.
    context.allocator = 0;

    // Setup Vulkan instance.
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.apiVersion = VK_API_VERSION_1_2;
    app_info.pApplicationName = application_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Kohi Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

//     // Required extensions for macOS (MoltenVK)
//     const char* extensions[] = {
//         "VK_KHR_surface",
// #if defined(KPLATFORM_APPLE)
//         "VK_EXT_metal_surface",
//         "VK_KHR_portability_enumeration"
// #elif defined(KPLATFORM_LINUX)
//         "VK_KHR_xcb_surface"
// #elif defined(KPLATFORM_WINDOWS)
//         "VK_KHR_win32_surface"
// #endif
//     };
//     u32 extension_count = sizeof(extensions) / sizeof(extensions[0]);

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    // obtain a list of required extensions
    const char** required_extensions = darray_create(const char*);
    darray_push(required_extensions, &VK_KHR_SURFACE_EXTENSION_NAME);  // generic surface extension
#if defined(KPLATFORM_APPLE)
    darray_push(required_extensions, &VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    darray_push(required_extensions, &VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#elif defined(KPLATFORM_LINUX)
    darray_push(required_extensions, &VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(KPLATFORM_WINDOWS)
    darray_push(required_extensions, &VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

#if defined(_DEBUG)
    darray_push(required_extensions, &VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    KDEBUG("Requested Vulkan extensions:");
    u64 req_ext_count = darray_length(required_extensions);
    for (u64 i = 0; i < req_ext_count; ++i) {
        KDEBUG("  %s", required_extensions[i]);
    }
#endif

    platform_get_required_extension_names(&required_extensions);// get platform-specific extensions
    u32 extension_count = (u32)darray_length(required_extensions);
    const char** extensions = required_extensions;

    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = 0;

#if defined(KPLATFORM_APPLE)
    // Required for MoltenVK on macOS
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkResult result = vkCreateInstance(&create_info, context.allocator, &context.instance);
    if (result != VK_SUCCESS) {
        KERROR("vkCreateInstance failed with result: %u", result);
        return FALSE;
    }

    KINFO("Vulkan renderer initialized successfully.");
    return TRUE;
}

void vulkan_renderer_backend_shutdown(renderer_backend* backend) {
}

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height) {
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time) {
    return TRUE;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time) {
    return TRUE;
}