#include "GfxFrameBuffer.h"

void GfxFrameBuffer::Initialize(GfxRenderPass* newRenderPass, std::vector<GfxImage*>& newAttachments)
{
	renderPass = newRenderPass;
	attachments = newAttachments;

	std::vector<VkImageView> imageViewList(attachments.size());
	for (auto view : attachments)
	{
		imageViewList.push_back(view->GetImageView());
	}

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = renderPass->GetRenderPass();								// Render pass layout the framebuffer will be used with
	framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(imageViewList.size());
	framebufferCreateInfo.pAttachments = imageViewList.data();									// List of attachments (1:1 with render pass)
	framebufferCreateInfo.width = attachments[0]->GetWidth();									// framebuffer width
	framebufferCreateInfo.height = attachments[0]->GetHeight();									// framebuffer height
	framebufferCreateInfo.layers = 1;															// framebuffer layers

	width = framebufferCreateInfo.width;
	height = framebufferCreateInfo.height;

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
