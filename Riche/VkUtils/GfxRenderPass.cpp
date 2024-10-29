#include "GfxRenderPass.h"

void GfxSubpass::Initialize(GfxPipelineType type)
{
	this->type = type;
}

GfxSubpass& GfxSubpass::AddColorAttachmentReference(uint32_t attachment, VkImageLayout layout)
{
	VkAttachmentReference ref = {};
	ref.attachment = attachment;
	ref.layout = layout;

	colorAttachmentRefs.push_back(ref);

	return *this;
}

GfxSubpass& GfxSubpass::AddInputAttachmentReference(uint32_t attachment, VkImageLayout layout)
{
	VkAttachmentReference ref = {};
	ref.attachment = attachment;
	ref.layout = layout;

	inputAttachmentRefs.push_back(ref);

	return *this;
}

GfxSubpass& GfxSubpass::SetDepthStencilAttachmentReference(uint32_t attachment, VkImageLayout layout)
{
	depthStencilAttachmentRef.attachment = attachment;
	depthStencilAttachmentRef.layout = layout;

	return *this;
}

std::vector<VkAttachmentReference>& GfxSubpass::GetColorAttachmentReferences()
{
	return colorAttachmentRefs;
}

std::vector<VkAttachmentReference>& GfxSubpass::GetInputAttachmentReferences()
{
	return inputAttachmentRefs;
}

VkAttachmentReference& GfxSubpass::GetDepthStencilAttachmentReference()
{
	return depthStencilAttachmentRef;
}

GfxPipelineType GfxSubpass::GetPipelineType()
{
	return type;
}

GfxRenderPass::GfxRenderPass()
{
}

GfxRenderPass::~GfxRenderPass()
{
	Destroy();
}

void GfxRenderPass::Initialize(VkDevice newDevice, std::vector<GfxAttachmentDesc>& newAttachments, std::vector<GfxSubpass>& newSubpasses)
{
	this->device = newDevice;
	subpasses = newSubpasses;
	attachments = newAttachments;

	// Change GfxAttachmentDesc -> VkAttachmentDesc
	std::vector<VkAttachmentDescription> vkAttachmentDescs;
	for (auto& att : attachments)
	{
		VkAttachmentDescription desc = {};
		desc.format = att.format;
		desc.samples = att.samples;
		desc.loadOp = att.loadOp;
		desc.storeOp = att.storeOp;
		desc.stencilLoadOp = att.stencilLoadOp;
		desc.stencilStoreOp = att.stencilStoreOp;
		desc.initialLayout = att.initialLayout;
		desc.finalLayout = att.finalLayout;

		vkAttachmentDescs.push_back(desc);
	}

	uint32_t subpassIdx = 0;
	uint32_t subpassCount = static_cast<uint32_t>(subpasses.size());

	std::vector<VkSubpassDependency> subpassDependencies;

	if (subpassCount > 0)
	{
		VkSubpassDependency topSubpassDependency = {};
		// Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		// Transition must happen after ..
		topSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;						// 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
		topSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Pipeline stage
		topSubpassDependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;				// Stage access mask (memory access)
		// But most happen before ..
		topSubpassDependency.dstSubpass = 0;
		topSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		topSubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		topSubpassDependency.dependencyFlags = 0;

		subpassDependencies.push_back(topSubpassDependency);
	}

	std::vector<VkSubpassDescription> subpassDescriptions;
	for (GfxSubpass& subpass : subpasses)
	{
		VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		switch (subpass.GetPipelineType())
		{
		case GfxPipelineType::Graphics:
			pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			break;
		case GfxPipelineType::Compute:
			pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
			break;
		}

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = pipelineBindPoint;
		subpassDesc.colorAttachmentCount = static_cast<uint32_t>(subpass.GetColorAttachmentReferences().size());
		subpassDesc.pColorAttachments = subpass.GetColorAttachmentReferences().data();
		subpassDesc.inputAttachmentCount = static_cast<uint32_t>(subpass.GetInputAttachmentReferences().size());
		subpassDesc.pInputAttachments = subpass.GetInputAttachmentReferences().data();
		subpassDesc.pDepthStencilAttachment = &subpass.GetDepthStencilAttachmentReference();

		subpassDescriptions.push_back(subpassDesc);

		// if subpasses remain.
		if ((subpassIdx + 1) <= (subpassCount - 1))
		{
			VkSubpassDependency subpassDependency = {};
			subpassDependency.srcSubpass = subpassIdx;
			subpassDependency.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			subpassDependency.srcAccessMask = 0;

			subpassDependency.dstSubpass = subpassIdx + 1;
			subpassDependency.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			subpassDependency.dstAccessMask = 0;
			subpassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT; 
			// 'dependencyFlags = 0'을 사용하면, 이전 서브패스의 어떤 fragment shader로 기록된 버퍼에 대해서 접근가능하다.
			// 'dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT', 동일한 fragment shader에 대해 기록된 부분에 대해서 보장한다.

			subpassDependencies.push_back(subpassDependency);
		}
		subpassIdx += 1;
	}

	if (subpassCount > 0)
	{
		VkSubpassDependency endSubpassDependency = {};
		// Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		// Transition must happen after ..
		endSubpassDependency.srcSubpass = static_cast<uint32_t>(subpasses.size() - 1);						// 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
		endSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;	// Pipeline stage
		endSubpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;				// Stage access mask (memory access)
		// But most happen before ..
		endSubpassDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
		endSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		endSubpassDependency.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		endSubpassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		subpassDependencies.push_back(endSubpassDependency);
	}

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(vkAttachmentDescs.size());
	renderPassCreateInfo.pAttachments = vkAttachmentDescs.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
	renderPassCreateInfo.pSubpasses = subpassDescriptions.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to create RenderPass!");
	}
}

void GfxRenderPass::Destroy()
{
	vkDestroyRenderPass(device, renderPass, nullptr);
}

VkRenderPass GfxRenderPass::GetRenderPass()
{
	return renderPass;
}

std::vector<GfxSubpass>& GfxRenderPass::GetSubpasses()
{
	return subpasses;
}

std::vector<GfxAttachmentDesc>& GfxRenderPass::GetAttachments()
{
	return attachments;
}

VkDevice GfxRenderPass::GetDevice() const
{
	return device;
}
