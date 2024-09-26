#include "GfxImage.h"

GfxImage::GfxImage(VkDevice newDevice, VkPhysicalDevice newPhysicalDevice, VkImageCreateInfo createInfo, VkMemoryPropertyFlags memFlags) :
	device(newDevice), physicalDevice(newPhysicalDevice)
{
	VkResult result = vkCreateImage(device, &createInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create VkImage");
	}

	// CRAETE MEMORY FOR IMAGE
	// Get memory requirements for a type of image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, memFlags);

	result = vkAllocateMemory(device, &memAllocInfo, nullptr, &imageMemory);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Allocate An Image memory");
	}

	// Connect memory to Image
	vkBindImageMemory(device, image, imageMemory, 0);
}

GfxImage::~GfxImage()
{
	vkDestroyImage(device, image, nullptr);
	vkFreeMemory(device, imageMemory, nullptr);
}

VkImage GfxImage::GetImage() const
{
	return image;
}

VkDeviceMemory GfxImage::GetImageMemory() const
{
	return imageMemory;
}
