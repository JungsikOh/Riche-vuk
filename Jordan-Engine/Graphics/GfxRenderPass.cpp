#include "GfxRenderPass.h"

void GfxRenderPass::Initialize(VkDevice newDevice, std::vector<AttachmentDesc> colorAttchments, AttachmentDesc depthAttachment)
{
	colorAttachmentDescs = colorAttchments;
	depthAttachmentDesc = depthAttachment;

	std::vector<VkAttachmentReference> colorAttachmentRefs;
	uint32_t currAttachmentIdx = 0;

	std::vector<VkAttachmentDescription> attchmentDescs(colorAttachmentDescs.size());
	for (auto colorAttachmentDesc : colorAttachmentDescs)
	{
		VkAttachmentReference ref;
		ref.attachment = currAttachmentIdx++;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorAttachmentRefs.push_back(ref);

		VkAttachmentDescription desc;
		desc.format = colorAttachmentDesc.format;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.loadOp = colorAttachmentDesc.loadOp;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attchmentDescs.push_back(desc);
	}
}

GfxSubpass& GfxSubpass::AddColorAttachment(AttachmentDesc colorAttchment)
{
	VkAttachmentDescription attachmentDesc;
	attachmentDesc.format = colorAttchment.format;
	attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDesc.loadOp = colorAttchment.loadOp;
	attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	colorAttachmentDescs.push_back(colorAttchment);
}

GfxSubpass& GfxSubpass::AddInputReference(VkImageLayout layout)
{
	VkAttachmentReference inputRef{};
	inputRef.attachment = inputReferences.size();
	inputRef.layout = layout;

	inputReferences.push_back(inputRef);
}

GfxSubpass& GfxSubpass::SetDepthStencilAttachment(AttachmentDesc colorAttchment)
{
	// TODO: 여기에 return 문을 삽입합니다.
}
