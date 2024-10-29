#pragma once
#include "GfxCore.h"

class GfxRenderPass;
class GfxImage;


class GfxFrameBuffer
{
public:
	void Initialize(uint32_t newWidth, uint32_t newHeight, GfxRenderPass* newRenderPass, std::vector<VkImageView>& newAttachments);
	void Destroy();

	VkFramebuffer GetVkFrameBuffer();

private:
	/*VkDevice device;*/	// ����� GfxRenderPass�� ����� VkDevice �̿�.

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	GfxRenderPass* renderPass;
	std::vector<VkImageView> attachments;

	uint32_t width;
	uint32_t height;
};

