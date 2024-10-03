#include "GfxQueue.h"

GfxQueue::GfxQueue(VkDevice newDevice, uint32_t queueFamilyIndex, VkQueueFamilyProperties properties, uint32_t index) :
	device(newDevice), queueFamilyIndex(queueFamilyIndex), queueIndex(index), properties(properties)
{
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &handle);
}

GfxQueue::GfxQueue(GfxQueue&& other) :
	device(other.device), handle(other.handle), queueFamilyIndex(other.queueFamilyIndex), queueIndex(other.queueIndex), properties(other.properties)
{
	other.handle = VK_NULL_HANDLE;
	other.queueFamilyIndex = {};
	other.queueIndex = {};
	other.properties = {};
}

GfxQueue::~GfxQueue()
{
	device = nullptr;
}

VkQueue GfxQueue::GetHandle() const
{
	return handle;
}

uint32_t GfxQueue::GetQueueFamilyIndex() const
{
	return queueFamilyIndex;
}

uint32_t GfxQueue::GetQueueIndex() const
{
	return queueIndex;
}

