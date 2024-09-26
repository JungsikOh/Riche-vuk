#pragma once
#include "GfxCore.h"

class GfxImage
{
	friend class GfxDevice;

public:
	GfxImage(VkDevice newDevice, VkPhysicalDevice newPhysicalDevice, 
		VkImageCreateInfo createInfo, VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	~GfxImage();

	VkImage GetImage() const;
	VkDeviceMemory GetImageMemory() const;

private:
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	VkImage image;
	VkDeviceMemory imageMemory;

public:
	static VkImageCreateInfo ImageCreateInfo2D(VkExtent2D size, uint32_t mipsCount, uint32_t arrayLayersCount, VkFormat format, VkImageUsageFlags usage);
};

