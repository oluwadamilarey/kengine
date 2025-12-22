#pragma once
#include "defines.h"


/**
 * @brief
 *
 * appends to names__darray the required Vulkan extension names for the current platform.
 * @param names__darray
 */

void platform_get_required_extension_names(const char*** names__darray);
  