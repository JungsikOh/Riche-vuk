#include "GfxBuffer.h"

void GfxBuffer::Initialize(VkDevice newDevice, VkPhysicalDevice newPhysicalDevice)
{
	device = newDevice;
	physicalDevice = newPhysicalDevice;
}

void GfxBuffer::Destory()
{
	vkDestroyBuffer(device, buffer, nullptr);
	vkFreeMemory(device, bufferMemory, nullptr);
}

void* GfxBuffer::GetMappedData() const
{
	return mappedPtr;
}

VkBuffer GfxBuffer::GetVkBuffer() const
{
	return buffer;
}

VkDeviceMemory GfxBuffer::GetBufferMemory() const
{
	return bufferMemory;
}

uint32_t GfxBuffer::GetOffset() const
{
	return offset;
}

void* GfxBuffer::Map()
{
	if (mappedPtr) return mappedPtr;

	VkResult result = vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mappedPtr);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to map memory");
	}

	return mappedPtr;
}

void GfxBuffer::Unmap()
{
	vkUnmapMemory(device, bufferMemory);
	mappedPtr = nullptr;
}
