#pragma once

/* Graphics API Abstraction */
#include "typedef.h"
#include "GfxDevice.h"
#include "GfxImage.h"
#include "GfxBuffer.h"
#include "GfxDescriptor.h"
#include "GfxPipeline.h"
#include "GfxRenderPass.h"
#include "GfxFrameBuffer.h"
#include "GfxCommandPool.h"
#include "GfxCommandBuffer.h"
#include "GfxResourceManager.h"

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
		throw std::runtime_error("Failed to Create Buffer");
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
		assert(false && "Failed to alloc Buffer memory");
	}

	// Bind memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	// Create Buffer
	// Command buffer to hold transfer commands
	VkCommandBuffer transferCommandBuffer;

	// Command Buffer details
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer and pool
	VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command Buffer!");
	}

	// Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We're only using the command bufer once, so set up for one time submit. 

	// Begine recording transfer commands
	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	// Region of data to copy from and to
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	// End commands
	vkEndCommandBuffer(transferCommandBuffer);

	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	// Submit Transfer command to transfer queue and wait until it finishes
	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue); // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

	// Free Temporary command buffer back to pool
	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

static std::vector<char> readFile(const std::string& filename)
{
	// Open stream from given file
	// std::ios::binary tells stream to read file as binary
	// std::ios::ate tells stream to start reading from end of file
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	// Check if file stream successfully opened
	if (!file.is_open())
	{
		assert(false && "Failed to open a file!");
	}

	// Get current read position and use to resize file buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	// Move read position (seek to) the start of the file
	file.seekg(0);
	// Read the file data into the buffer (stream "fileSize" in total)
	file.read(fileBuffer.data(), fileSize);

	// Close stream
	file.close();

	return fileBuffer;
}