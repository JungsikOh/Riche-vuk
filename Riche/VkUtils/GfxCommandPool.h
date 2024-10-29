#pragma once

#include "GfxCore.h"

class GfxCommandBuffer;

class GfxCommandPool
{
public:
	GfxCommandPool() = default;
	~GfxCommandPool() = default;

	GfxCommandPool& Initialize(VkDevice newDevice, uint32_t newQueueFamilyIndex);
	void Destroy();
	
	GfxCommandBuffer* Allocate();
	bool Free(GfxCommandBuffer* commandBuffer);

	const uint32_t GetQueueFamily();
	VkCommandPool& GetVkCommandPool();

private:
	VkDevice device;
	uint32_t queueFamilyIndex;
	VkCommandPool commandPool;

	std::vector<std::shared_ptr<GfxCommandBuffer>> commandBuffers;

};

