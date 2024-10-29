#include "GfxImage.h"

void GfxImage::Initialize(VkDevice newDevice, VkPhysicalDevice newPhysicalDevice)
{
	device = newDevice;
	physicalDevice = physicalDevice;
}

void GfxImage::Destroy()
{
	vkDestroyImage(device, image, nullptr);
	vkFreeMemory(device, imageMemory, nullptr);
}

void GfxImage::SetImage(VkImageCreateInfo createInfo, VkMemoryPropertyFlags memFlags)
{
	width = createInfo.extent.width;
	height = createInfo.extent.height;

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

void GfxImage::SetImageView(VkImageViewCreateInfo createInfo)
{
	createInfo.image = image;
	VkResult result = vkCreateImageView(device, &createInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create VkImage");
	}
}

VkImage& GfxImage::GetImage()
{
	return image;
}

VkImageView& GfxImage::GetImageView()
{
	return imageView;
}

VkDeviceMemory& GfxImage::GetImageMemory()
{
	return imageMemory;
}

uint32_t GfxImage::GetWidth() const
{
	return width;
}

uint32_t GfxImage::GetHeight() const
{
	return height;
}
