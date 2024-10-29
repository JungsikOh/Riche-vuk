#include "GfxFrameBuffer.h"

void GfxFrameBuffer::Initialize(uint32_t newWidth, uint32_t newHeight, GfxRenderPass* newRenderPass, std::vector<VkImageView>& newAttachments)
{
	width = newWidth;
	height = newHeight;
	renderPass = newRenderPass;
	attachments = newAttachments;

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = renderPass->GetRenderPass();								// Render pass layout the framebuffer will be used with
	framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferCreateInfo.pAttachments = attachments.data();									// List of attachments (1:1 with render pass)
	framebufferCreateInfo.width = width;									// framebuffer width
	framebufferCreateInfo.height = height;									// framebuffer height
	framebufferCreateInfo.layers = 1;															// framebuffer layers

	VkResult result = vkCreateFramebuffer(renderPass->GetDevice(), &framebufferCreateInfo, nullptr, &framebuffer);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create Frame buffer!");
	}
}

void GfxFrameBuffer::Destroy()
{
	attachments.clear();
	vkDestroyFramebuffer(renderPass->GetDevice(), framebuffer, nullptr);

	renderPass = nullptr;
}

VkFramebuffer GfxFrameBuffer::GetVkFrameBuffer()
{
	return framebuffer;
}
