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
	/*VkDevice device;*/	// 연결된 GfxRenderPass를 사용해 VkDevice 이용.

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	GfxRenderPass* renderPass;
	std::vector<VkImageView> attachments;

	uint32_t width;
	uint32_t height;
};

