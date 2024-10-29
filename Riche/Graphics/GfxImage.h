#pragma once
#include "GfxCore.h"

class GfxImage
{
public:
	GfxImage() = default;
	~GfxImage() = default;

	void Initialize(VkDevice newDevice, VkPhysicalDevice newPhysicalDevice);
	void Destroy();

	void SetImage(VkImageCreateInfo createInfo, VkMemoryPropertyFlags memFlags);
	void SetImageView(VkImageViewCreateInfo createInfo);

	VkImage& GetImage();
	VkImageView& GetImageView();
	VkDeviceMemory& GetImageMemory();

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

private:
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	// Core
	VkImage image;
	VkImageView imageView;
	VkDeviceMemory imageMemory;

	uint32_t width;
	uint32_t height;
};

static VkImageCreateInfo ImageCreateInfo2D(uint32_t width, uint32_t height, uint32_t mipsCount, uint32_t arrayLayersCount,
	VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;							// Type of Image (1D, 2D or 3D)
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = mipsCount;									// Number of mipmap levels
	imageCreateInfo.arrayLayers = arrayLayersCount;							// Number of levels in image array
	imageCreateInfo.format = format;										// Format type of image
	imageCreateInfo.tiling = tiling;										// How image data should be "tiled" (arranged for optimal reading)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;				// Layout of image data on creation (프레임버퍼에 맞게 변형됨)
	imageCreateInfo.usage = usage;											// Bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;						// Number of samples for multi-sampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;				// Whether image can be shared between queues

	return imageCreateInfo;
}

static VkImageViewCreateInfo ImageViewCreateInfo2D(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;											// image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (1D, 2D, 3D, Cube, etc)
	viewCreateInfo.format = format;											// Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping of rgba components to rgba values
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;				// Which aspect of image to view (e.g. COLOR_BIT for viewing color)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;						// Start Mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;							// Number of mipmap levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;						// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;							// Number of array levels to view

}
