#pragma once
#include "GfxDevice.h"

/// Queue encapsulates a single vkQueue, used to submit vulkan commands for processing.
class GfxDevice;

class GfxQueue
{
public:
	GfxQueue(GfxDevice& device, uint32_t queueFamilyIndex, VkQueueFamilyProperties properties, uint32_t index);
	GfxQueue(const GfxQueue&) = default;
	GfxQueue(GfxQueue&& other);

	GfxQueue& operator=(const GfxQueue&) = delete;
	GfxQueue& operator=(GfxQueue&&) = delete;

	~GfxQueue() = default;

	VkQueue GetHandle() const;
	uint32_t GetQueueFamilyIndex() const;
	uint32_t GetQueueIndex() const;
	//const VkQueueFamilyProperties GetProperties() const;

protected:
	GfxDevice& device;

	VkQueue handle{ VK_NULL_HANDLE };

	uint32_t queueFamilyIndex;
	uint32_t queueIndex;
	VkQueueFamilyProperties properties;
};

