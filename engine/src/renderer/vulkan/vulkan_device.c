#include "vulkan_device.h"
#include "core/logger.h"
#include "core/kstring.h"
#include "containers/darray.h"
#include "core/kmemory.h"

typedef struct vulkan_physical_device_requirements {
    b8 graphics;
    b8 present;
    b8 compute;
    b8 transfer;
    // darray
    const char** device_extension_names;
    b8 sampler_anisotropy;
    b8 discrete_gpu;
} vulkan_physical_device_requirements;

typedef struct vulkan_queue_family_indices {
    u32 graphics_index;
    u32 present_index;
    u32 compute_index;
    u32 transfer_index;
    b8 graphics_found;
    b8 present_found;
    b8 compute_found;
    b8 transfer_found;
} vulkan_queue_family_indices;

typedef struct vulkan_physical_device_queue_family_info {
    u32 graphics_family_index;
    u32 present_family_index;
    u32 compute_family_index;
    u32 transfer_family_index;
} vulkan_physical_device_queue_family_info;

b8 select_physical_device(vulkan_context* context);
b8 physical_device_meets_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties* properties,
    const VkPhysicalDeviceFeatures* features,
    const vulkan_physical_device_requirements* requirements,
    vulkan_physical_device_queue_family_info* out_queue_info,
    vulkan_swapchain_support_info* out_swapchain_support);

b8 vulkan_device_create(vulkan_context* context) {
    KINFO("Selecting Vulkan physical device...");
    if (!select_physical_device(context)) {
        KFATAL("Failed to select a suitable Vulkan physical device.");
        return FALSE;
    }
    KINFO("Vulkan physical device selected successfully.");

    KINFO("Creating logical device...");

    // Determine unique queue families
    b8 present_shares_graphics_queue = context->device.graphics_queue_index == context->device.present_queue_index;
    b8 transfer_shares_graphics_queue = context->device.graphics_queue_index == context->device.transfer_queue_index;

    u32 index_count = 1;
    if (!present_shares_graphics_queue) {
        index_count++;
    }
    if (!transfer_shares_graphics_queue) {
        index_count++;
    }

    u32 indices[32];  // Use fixed size array to avoid VLA
    u8 index = 0;
    indices[index++] = context->device.graphics_queue_index;
    if (!present_shares_graphics_queue) {
        indices[index++] = context->device.present_queue_index;
    }
    if (!transfer_shares_graphics_queue) {
        indices[index++] = context->device.transfer_queue_index;
    }

    VkDeviceQueueCreateInfo queue_create_infos[32];
    for (u32 i = 0; i < index_count; ++i) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = indices[i];
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].pNext = 0;

        // Query actual queue count for this family
        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context->device.physical_device, &queue_family_count, 0);
        VkQueueFamilyProperties* queue_families = kallocate(
            sizeof(VkQueueFamilyProperties) * queue_family_count,
            MEMORY_TAG_RENDERER);
        vkGetPhysicalDeviceQueueFamilyProperties(context->device.physical_device, &queue_family_count, queue_families);

        // Get the actual available queue count for this family
        u32 available_queue_count = queue_families[indices[i]].queueCount;
        kfree(queue_families, sizeof(VkQueueFamilyProperties) * queue_family_count, MEMORY_TAG_RENDERER);

        // On M1, most queue families only have 1 queue available
        // Request the minimum of what we want vs what's available
        queue_create_infos[i].queueCount = 1;  

        KINFO("Queue family %d: requesting %d queue(s) (available: %d)",
              indices[i], queue_create_infos[i].queueCount, available_queue_count);

        f32 queue_priority = 1.0f;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }

    // Request device features
    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;

    // CRITICAL FIX: Check if portability subset extension is available
    u32 available_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        context->device.physical_device,
        NULL,
        &available_extension_count,
        NULL);

    VkExtensionProperties* available_extensions = kallocate(
        sizeof(VkExtensionProperties) * available_extension_count,
        MEMORY_TAG_RENDERER);
    vkEnumerateDeviceExtensionProperties(
        context->device.physical_device,
        NULL,
        &available_extension_count,
        available_extensions);

    // Check for portability subset (required on MoltenVK)
    b8 portability_subset_available = FALSE;
    for (u32 i = 0; i < available_extension_count; ++i) {
        if (strings_equal(available_extensions[i].extensionName, "VK_KHR_portability_subset")) {
            portability_subset_available = TRUE;
            KINFO("VK_KHR_portability_subset extension detected (MoltenVK)");
            break;
        }
    }
    kfree(available_extensions, sizeof(VkExtensionProperties) * available_extension_count, MEMORY_TAG_RENDERER);

    // Build extension list
    const char* extension_names[8];  // Fixed size array
    u32 extension_count = 0;

    extension_names[extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    if (portability_subset_available) {
        extension_names[extension_count++] = "VK_KHR_portability_subset";
    }

    KINFO("Enabling %d device extension(s):", extension_count);
    for (u32 i = 0; i < extension_count; ++i) {
        KINFO("  %s", extension_names[i]);
    }

    VkDeviceCreateInfo device_create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_create_info.queueCreateInfoCount = index_count;
    device_create_info.pQueueCreateInfos = queue_create_infos;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = extension_count;
    device_create_info.ppEnabledExtensionNames = extension_names;

    // Validation layers deprecated at device level, but set for compatibility
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = 0;

    // Create the device
    VkResult result = vkCreateDevice(
        context->device.physical_device,
        &device_create_info,
        context->allocator,
        &context->device.logical_device);

    if (result != VK_SUCCESS) {
        KFATAL("Failed to create logical device. VkResult: %d", result);
        return FALSE;
    }

    KINFO("Logical device created.");

    // Get queues - always use queue index 0 since we only requested 1 per family
    vkGetDeviceQueue(
        context->device.logical_device,
        context->device.graphics_queue_index,
        0,  // Queue index within the family
        &context->device.graphics_queue);

    vkGetDeviceQueue(
        context->device.logical_device,
        context->device.present_queue_index,
        0,
        &context->device.present_queue);

    vkGetDeviceQueue(
        context->device.logical_device,
        context->device.transfer_queue_index,
        0,
        &context->device.transfer_queue);

    KINFO("Queues obtained:");
    KINFO("  Graphics queue: family %d, queue 0", context->device.graphics_queue_index);
    KINFO("  Present queue:  family %d, queue 0", context->device.present_queue_index);
    KINFO("  Transfer queue: family %d, queue 0", context->device.transfer_queue_index);

    return TRUE;
}

void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    vulkan_swapchain_support_info* out_support_info) {
    // Initialize to zero
    out_support_info->format_count = 0;
    out_support_info->present_mode_count = 0;
    out_support_info->formats = NULL;
    out_support_info->present_modes = NULL;

    // Query surface capabilities with error handling
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device,
        surface,
        &out_support_info->capabilities);

    if (result != VK_SUCCESS) {
        KERROR("Failed to query surface capabilities (VkResult: %d)", result);
        return;
    }

    // Query surface formats
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device,
        surface,
        &out_support_info->format_count,
        NULL);

    if (result != VK_SUCCESS) {
        KERROR("Failed to query surface format count (VkResult: %d)", result);
        out_support_info->format_count = 0;
        return;
    }

    if (out_support_info->format_count > 0) {
        out_support_info->formats = kallocate(
            sizeof(VkSurfaceFormatKHR) * out_support_info->format_count,
            MEMORY_TAG_RENDERER);

        result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &out_support_info->format_count,
            out_support_info->formats);

        if (result != VK_SUCCESS) {
            KERROR("Failed to query surface formats (VkResult: %d)", result);
            kfree(out_support_info->formats,
                  sizeof(VkSurfaceFormatKHR) * out_support_info->format_count,
                  MEMORY_TAG_RENDERER);
            out_support_info->formats = NULL;
            out_support_info->format_count = 0;
            return;
        }
    }

    // Query present modes
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device,
        surface,
        &out_support_info->present_mode_count,
        NULL);

    if (result != VK_SUCCESS) {
        KERROR("Failed to query present mode count (VkResult: %d)", result);
        out_support_info->present_mode_count = 0;
        return;
    }

    if (out_support_info->present_mode_count > 0) {
        out_support_info->present_modes = kallocate(
            sizeof(VkPresentModeKHR) * out_support_info->present_mode_count,
            MEMORY_TAG_RENDERER);

        result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &out_support_info->present_mode_count,
            out_support_info->present_modes);

        if (result != VK_SUCCESS) {
            KERROR("Failed to query present modes (VkResult: %d)", result);
            kfree(out_support_info->present_modes,
                  sizeof(VkPresentModeKHR) * out_support_info->present_mode_count,
                  MEMORY_TAG_RENDERER);
            out_support_info->present_modes = NULL;
            out_support_info->present_mode_count = 0;
            return;
        }
    }

    KDEBUG("Swapchain support queried: %d formats, %d present modes",
           out_support_info->format_count,
           out_support_info->present_mode_count);
}

b8 vulkan_device_detect_depth_format(
    vulkan_device* device) {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (u32 i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            device->depth_format = candidates[i];
            KINFO("Selected depth format: %d", device->depth_format);
            return TRUE;
        }
    }

    KFATAL("Failed to find a supported depth format.");
    return FALSE;
}

void vulkan_device_destroy(vulkan_context* context) {
    // Unset queues
    context->device.graphics_queue = 0;
    context->device.present_queue = 0;
    context->device.transfer_queue = 0;

    // Destroy logical device
    KINFO("Destroying logical device...");
    if (context->device.logical_device) {
        vkDestroyDevice(context->device.logical_device, context->allocator);
        context->device.logical_device = 0;
    }

    // Physical devices are not destroyed.
    KINFO("Releasing physical device resources...");
    context->device.physical_device = 0;

    if (context->device.swapchain_support.formats) {
        kfree(
            context->device.swapchain_support.formats,
            sizeof(VkSurfaceFormatKHR) * context->device.swapchain_support.format_count,
            MEMORY_TAG_RENDERER);
        context->device.swapchain_support.formats = 0;
        context->device.swapchain_support.format_count = 0;
    }

    if (context->device.swapchain_support.present_modes) {
        kfree(
            context->device.swapchain_support.present_modes,
            sizeof(VkPresentModeKHR) * context->device.swapchain_support.present_mode_count,
            MEMORY_TAG_RENDERER);
        context->device.swapchain_support.present_modes = 0;
        context->device.swapchain_support.present_mode_count = 0;
    }

    kzero_memory(
        &context->device.swapchain_support.capabilities,
        sizeof(context->device.swapchain_support.capabilities));

    context->device.graphics_queue_index = -1;
    context->device.present_queue_index = -1;
    context->device.transfer_queue_index = -1;
}

b8 select_physical_device(vulkan_context* context) {
    u32 physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, 0));

    if (physical_device_count == 0) {
        KERROR("No Vulkan physical devices found.");
        return FALSE;
    }

    VkPhysicalDevice physical_devices[physical_device_count];
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices));

    for (u32 i = 0; i < physical_device_count; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &memory);

        vulkan_physical_device_requirements requirements = {};
        requirements.graphics = TRUE;
        requirements.present = TRUE;
        requirements.compute = TRUE;
        requirements.transfer = TRUE;
        requirements.sampler_anisotropy = TRUE;
        requirements.discrete_gpu = FALSE;

        const char* required_device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        requirements.device_extension_names = darray_create(const char*);
        for (u32 j = 0; j < sizeof(required_device_extensions) / sizeof(required_device_extensions[0]); ++j) {
            darray_push(requirements.device_extension_names, required_device_extensions[j]);
        }

        vulkan_physical_device_queue_family_info queue_info = {};

        b8 result = physical_device_meets_requirements(
            physical_devices[i],
            context->surface,
            &properties,
            &features,
            &requirements,
            &queue_info,
            &context->device.swapchain_support);

        if (result) {
            KINFO("Selected device: '%s'.", properties.deviceName);
            switch (properties.deviceType) {
                default:
                case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                    KINFO("GPU type is Unknown.");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    KINFO("GPU type is Integrated.");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    KINFO("GPU type is Discrete."); 
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    KINFO("GPU type is Virtual.");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    KINFO("GPU type is CPU.");
                    break;
            }
            KINFO(
                "GPU Driver version: %d.%d.%d",
                VK_VERSION_MAJOR(properties.driverVersion),
                VK_VERSION_MINOR(properties.driverVersion),
                VK_VERSION_PATCH(properties.driverVersion));
            KINFO(
                "Vulkan API version: %d.%d.%d",
                VK_VERSION_MAJOR(properties.apiVersion),
                VK_VERSION_MINOR(properties.apiVersion),
                VK_VERSION_PATCH(properties.apiVersion));

            for (u32 j = 0; j < memory.memoryHeapCount; ++j) {
                f32 memory_size_gib = (((f32)memory.memoryHeaps[j].size) / 1024.0f / 1024.0f / 1024.0f);
                if (memory.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    KINFO("Local GPU memory: %.2f GiB", memory_size_gib);
                } else {
                    KINFO("Shared System memory: %.2f GiB", memory_size_gib);
                }
            }

            context->device.physical_device = physical_devices[i];
            context->device.graphics_queue_index = queue_info.graphics_family_index;
            context->device.present_queue_index = queue_info.present_family_index;
            context->device.transfer_queue_index = queue_info.transfer_family_index;

            context->device.properties = properties;
            context->device.features = features;
            context->device.memory = memory;

            darray_destroy(requirements.device_extension_names);
            return TRUE;  // Fix 5: Added explicit return on success
        }

        darray_destroy(requirements.device_extension_names);
    }

    // Fix 5: Explicit return when no device found
    KERROR("No physical devices were found which meet the requirements.");
    return FALSE;
}

b8 physical_device_meets_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties* properties,
    const VkPhysicalDeviceFeatures* features,
    const vulkan_physical_device_requirements* requirements,
    vulkan_physical_device_queue_family_info* out_queue_info,
    vulkan_swapchain_support_info* out_swapchain_support) {
    // Initialize queue family indices
    out_queue_info->graphics_family_index = -1;
    out_queue_info->present_family_index = -1;
    out_queue_info->compute_family_index = -1;
    out_queue_info->transfer_family_index = -1;

    // Check discrete GPU requirement
    if (requirements->discrete_gpu) {
        if (properties->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            KINFO("Physical device '%s' rejected: not a discrete GPU.", properties->deviceName);
            return FALSE;
        }
    }

    // Query queue families
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);

    if (queue_family_count == 0) {
        KWARN("Physical device '%s' has no queue families", properties->deviceName);
        return FALSE;
    }

    VkQueueFamilyProperties* queue_families = darray_reserve(VkQueueFamilyProperties, queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    KINFO("Evaluating %d queue families for device '%s'...", queue_family_count, properties->deviceName);

    // Track best transfer queue (prefer dedicated transfer queues)
    // Score system: lower is better (dedicated queues have fewer capabilities)
    i32 min_transfer_score = 255;  // Start with max value

    for (u32 i = 0; i < queue_family_count; ++i) {
        VkQueueFamilyProperties* queue_family = &queue_families[i];
        i32 current_transfer_score = 0;

        // Check graphics support
        if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out_queue_info->graphics_family_index = i;
            current_transfer_score++;
            KDEBUG("  Queue family %d: Graphics support found", i);
        }

        // Check compute support
        if (queue_family->queueFlags & VK_QUEUE_COMPUTE_BIT) {
            out_queue_info->compute_family_index = i;
            current_transfer_score++;
            KDEBUG("  Queue family %d: Compute support found", i);
        }

        // Check transfer support
        if (queue_family->queueFlags & VK_QUEUE_TRANSFER_BIT) {
            current_transfer_score++;
            KDEBUG("  Queue family %d: Transfer support found (score: %d)", i, current_transfer_score);
            // Select transfer queue with lowest score (most dedicated)
            // On M1, all queues have graphics+compute+transfer, so just pick the first one
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                out_queue_info->transfer_family_index = i;
                KINFO("  Queue family %d selected as transfer queue (score: %d)", i, current_transfer_score);
            }
        }

        // Check present support - WITH ERROR HANDLING
        if (requirements->present && surface) {
            VkBool32 present_support = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (result != VK_SUCCESS) {
                KERROR("  Queue family %d: Failed to query present support (VkResult: %d)", i, result);
                // Don't immediately fail - surface might not be ready yet
                // This is common on macOS during initialization
                continue;
            }
            if (present_support) {
                out_queue_info->present_family_index = i;
                KINFO("  Queue family %d: Present support found", i);
            } else {
                KDEBUG("  Queue family %d: No present support", i);
            }
        }
    }

    // Log summary
    KINFO("Device '%s' queue family summary:", properties->deviceName);
    KINFO("  Graphics: %s (index: %d)",
          out_queue_info->graphics_family_index != -1 ? "YES" : "NO",
          out_queue_info->graphics_family_index);
    KINFO("  Present:  %s (index: %d)",
          out_queue_info->present_family_index != -1 ? "YES" : "NO",
          out_queue_info->present_family_index);
    KINFO("  Compute:  %s (index: %d)",
          out_queue_info->compute_family_index != -1 ? "YES" : "NO",
          out_queue_info->compute_family_index);
    KINFO("  Transfer: %s (index: %d)",
          out_queue_info->transfer_family_index != -1 ? "YES" : "NO",
          out_queue_info->transfer_family_index);

    // Validate required queue families were found
    if (requirements->graphics && out_queue_info->graphics_family_index == -1) {
        KINFO("Device '%s' rejected: No graphics queue family", properties->deviceName);
        return FALSE;
    }

    if (requirements->present && out_queue_info->present_family_index == -1) {
        KINFO("Device '%s' rejected: No present queue family", properties->deviceName);
        return FALSE;
    }

    if (requirements->compute && out_queue_info->compute_family_index == -1) {
        KINFO("Device '%s' rejected: No compute queue family", properties->deviceName);
        return FALSE;
    }

    if (requirements->transfer && out_queue_info->transfer_family_index == -1) {
        KINFO("Device '%s' rejected: No transfer queue family", properties->deviceName);
        return FALSE;
    }

    // Query swapchain support
    KDEBUG("Querying swapchain support...");
    vulkan_device_query_swapchain_support(device, surface, out_swapchain_support);

    if (out_swapchain_support->format_count < 1 || out_swapchain_support->present_mode_count < 1) {
        KINFO("Device '%s' rejected: Insufficient swapchain support", properties->deviceName);
        KDEBUG("  Formats: %d, Present modes: %d",
               out_swapchain_support->format_count,
               out_swapchain_support->present_mode_count);

        // Clean up allocated memory
        if (out_swapchain_support->formats) {
            kfree(out_swapchain_support->formats,
                  sizeof(VkSurfaceFormatKHR) * out_swapchain_support->format_count,
                  MEMORY_TAG_RENDERER);
            out_swapchain_support->formats = NULL;
            out_swapchain_support->format_count = 0;
        }
        if (out_swapchain_support->present_modes) {
            kfree(out_swapchain_support->present_modes,
                  sizeof(VkPresentModeKHR) * out_swapchain_support->present_mode_count,
                  MEMORY_TAG_RENDERER);
            out_swapchain_support->present_modes = NULL;
            out_swapchain_support->present_mode_count = 0;
        }
        return FALSE;
    }

    KINFO("  Swapchain support: %d formats, %d present modes",
          out_swapchain_support->format_count,
          out_swapchain_support->present_mode_count);

    // Check device extensions
    if (requirements->device_extension_names) {
        u32 available_extension_count = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, NULL));

        if (available_extension_count == 0) {
            KINFO("Device '%s' rejected: No extensions available", properties->deviceName);
            return FALSE;
        }

        VkExtensionProperties* available_extensions = kallocate(
            sizeof(VkExtensionProperties) * available_extension_count,
            MEMORY_TAG_RENDERER);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(
            device, NULL, &available_extension_count, available_extensions));

        u32 required_extension_count = darray_length(requirements->device_extension_names);
        KDEBUG("Checking %d required extensions against %d available extensions",
               required_extension_count, available_extension_count);

        for (u32 i = 0; i < required_extension_count; ++i) {
            b8 found = FALSE;
            const char* required_ext = requirements->device_extension_names[i];
 
            for (u32 j = 0; j < available_extension_count; ++j) {
                if (strings_equal(required_ext, available_extensions[j].extensionName)) {
                    found = TRUE;
                    KDEBUG("  Extension '%s' found", required_ext);
                    break;
                }
            }

            if (!found) {
                KINFO("Device '%s' rejected: Required extension '%s' not found",
                      properties->deviceName, required_ext);
                kfree(available_extensions,
                      sizeof(VkExtensionProperties) * available_extension_count,
                      MEMORY_TAG_RENDERER);
                return FALSE;
            }
        }

        kfree(available_extensions,
              sizeof(VkExtensionProperties) * available_extension_count,
              MEMORY_TAG_RENDERER);
    }

    // Check sampler anisotropy
    if (requirements->sampler_anisotropy && !features->samplerAnisotropy) {
        KINFO("Device '%s' rejected: samplerAnisotropy not supported", properties->deviceName);
        return FALSE;
    }

    KINFO("Device '%s' meets all requirements!", properties->deviceName);
    return TRUE;
}
