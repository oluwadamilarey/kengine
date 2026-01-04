#include "vulkan_backend.h"

#include "vulkan_types.inl"

#include "core/logger.h"
#include "core/kstring.h"

#include "containers/darray.h"
#include "platform/platform.h"
#include "vulkan_platform.h"
#include "core/kstring.h"
#include "vulkan_device.h"

// static Vulkan context
static vulkan_context context;

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

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
    platform_get_required_extension_names(&required_extensions);  // get platform-specific extension(s)
    u32 extension_count = (u32)darray_length(required_extensions);
    const char** extensions = required_extensions;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = (const char* const*)extensions;
    // validation layers
    const char** required_validation_layer_names = 0;
    u32 required_validation_layer_count = 0;
#if defined(_DEBUG)
    KINFO("Validation layers enabled. Enumerating...");
    // Create darray of const char* pointers
    required_validation_layer_names = darray_create(const char*);
    const char* khronos_validation = "VK_LAYER_KHRONOS_validation";
    darray_push(required_validation_layer_names, khronos_validation);
    required_validation_layer_count = darray_length(required_validation_layer_names);
    // Obtain a list of available validation layers
    u32 available_layer_count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    VkLayerProperties* available_layers = darray_reserve(VkLayerProperties, available_layer_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));
    // Verify all required layers are available.
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
            KFATAL("Required validation layer is missing: %s", required_validation_layer_names[i]);
            return FALSE;
        }
    }
    KINFO("All required validation layers are present.");
#endif
    create_info.enabledLayerCount = required_validation_layer_count;
    create_info.ppEnabledLayerNames = (const char* const*)required_validation_layer_names;
#if defined(KPLATFORM_APPLE)
    // Required for MoltenVK on macOS
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    VK_CHECK(vkCreateInstance(&create_info, context.allocator, &context.instance));
    // Debugger
#if defined(_DEBUG)
    KDEBUG("Creating Vulkan debugger...");
    u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;  //|
                                                                      //    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vk_debug_callback;
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");
    KASSERT_MSG(func, "Failed to create debug messenger!");
    VK_CHECK(func(context.instance, &debug_create_info, context.allocator, &context.debug_messenger));
    KDEBUG("Vulkan debugger created.");
#endif
    //surface creation
    if (!platform_create_vulkan_surface(plat_state, &context)) {
        KFATAL("Failed to create Vulkan surface.");
        return FALSE;
    }
    //device creation 
    if (!vulkan_device_create(&context)) {
        KFATAL("Failed to create Vulkan device.");
        return FALSE;
    }
    KINFO("Vulkan renderer initialized successfully.");
    return TRUE;
}
void vulkan_renderer_backend_shutdown(renderer_backend* backend) {
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
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time) {
    return TRUE;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time) {
    return TRUE;
}

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