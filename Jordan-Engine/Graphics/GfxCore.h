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

/* Graphics API Abstraction */
#include "GfxDevice.h"
#include "GfxQueue.h"
#include "GfxImage.h"
#include "GfxBuffer.h"
#include "GfxDescriptor.h"
#include "GfxDescriptorManager.h"

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

static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	// CREATE VERTEX BUFFER
	// Information to create a buffer (doesn't include assigning memory)
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = bufferSize; // size of buffer (size of 1 vertex * number of vertices)
	bufferCreateInfo.usage = bufferUsage;			// Multiple types of buffer possible, we want vertex buffer
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Similar to Swapchain images, can share vertex buffers

	// In Vulkan Cookbook 166p, Buffers don't have their own memory.
	VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to Create Vertex Buffer");
	}

	// GET BUFFER MEMORY REQUIRMENTS
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	// ALLOCATE MEMORY TO BUFFER
	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits,					// Index of memory type on physical device that has required bit flags
		bufferProperties);																								// VK_MEMORY_.._HOST_VISIBLE_BIT : CPU can interact with memory(GPU)
																														// VK_MEMORY_.._HOST_COHERENT_BIT : Allows placement of data straight into buffer after mapping (otherwise would have to specify maually)
	// Allocate Memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memAllocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to alloc Vertex Buffer memory");
	}

	// Bind memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}