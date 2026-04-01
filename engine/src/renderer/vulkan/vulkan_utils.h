#pragma once
#include "vulkan_types.inl"

/**
 * returns the string representation of result
 * @param result The result to get the string for
 * @param get_extended indicates whether to also return an extended result
 * @returns The error code and/or extended error message in string form. defaults to success for unknown result types.
 *
 */
const char* vulkan_result_string(VkResult result, b8 get_extended);

/**
 *indicates if the passed result is a success or an error as defined by the vulkan spec
 * @returns True if success, otherwise false Defaukts to true for unknown result type
 */
b8 vulkan_result_is_success(VkResult result);