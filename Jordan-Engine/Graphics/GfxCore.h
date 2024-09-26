#pragma once

#include <iostream>
#include <array>
#include <vector>
#include <optional>
#include <mutex>
#include <string>
#include <set>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <assert.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "GfxDevice.h"
#include "GfxQueue.h"
#include "GfxImage.h"

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	// Get properties of physical device memory This call will store memory parameters (number of heaps, their size, and types) of the physical device used for processing.
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if ((allowedTypes & (1 << i))																// index of memory type must match corresponding bit in allowedTypes
			&& (memProperties.memoryTypes[i].propertyFlags & properties) == properties)				// Desired property bit flags are part of memory type's property flags
		{
			// This memory type is vaild, so return its index
			return i;
		}
	}
}