#pragma once
#include "GfxCore.h"

class GfxBuffer
{
public:
	GfxBuffer() = default;
	~GfxBuffer() = default;

	void Initialize(VkDevice newDevice, VkPhysicalDevice newPhysicalDevice);
	void Destory();

	void* GetMappedData();
	VkBuffer& GetVkBuffer();
	VkDeviceMemory& GetBufferMemory();
	uint32_t GetOffset();

	void* Map();
	void Unmap();

private:
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	// Core
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	// Desc
	VkDeviceSize bufferSize;
	VkBufferUsageFlags bufferUsage;
	VkMemoryPropertyFlags bufferProperties;
	uint32_t stride = 0;
	uint32_t offset = 0;

	void* mappedPtr = nullptr;
};

