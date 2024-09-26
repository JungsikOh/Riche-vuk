#include "GfxImage.h"

GfxImage::GfxImage(GfxDevice& device, VkImageCreateInfo createInfo, VkMemoryPropertyFlags memFlags) :
	device(device)
{
	VkResult result = vkCreateImage(device.GetDevice(), &createInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create VkImage");
	}

	// CRAETE MEMORY FOR IMAGE
	// Get memory requirements for a type of image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device.GetDevice(), image, &memRequirements);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(device.GetPhysicalDevice(), memRequirements.memoryTypeBits, memFlags);

	result = vkAllocateMemory(device.GetDevice(), &memAllocInfo, nullptr, &imageMemory);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Allocate An Image memory");
	}

	// Connect memory to Image
	vkBindImageMemory(device.GetDevice(), image, imageMemory, 0);
}

GfxImage::~GfxImage()
{
	vkDestroyImage(device.GetDevice(), image, nullptr);
	vkFreeMemory(device.GetDevice(), imageMemory, nullptr);
}

VkImage GfxImage::GetImage() const
{
	return image;
}

VkDeviceMemory GfxImage::GetImageMemory() const
{
	return imageMemory;
}
