#pragma once

#include "GfxCore.h"

// VULKAN INFO
// Attachment vs Attachment Reference
// Attachment: 렌더링 동안 사용할 리소스(예, colour, depth, stencil, etc)를 설명하는 구조체.
//			렌더링에 사용될 이미지의 속성을 정의하고 여기에는 format, number of sample, loadOp, storeOp, inital/final layout
// Attachment Reference: 특정 Subpass에서 어떤 Attachment가 사용될지를 지정하는 구조체.
//						Attachment가 이미지의 속성을 설명하는 반면, Ref는 서브패스 내에서 이러한 Attachment가 어떻게 사용(읽기/쓰기)되는지를 지정.

struct AttachmentDesc
{
	VkFormat format;
	VkAttachmentLoadOp loadOp;
	VkClearValue clearValue;

	bool operator <(const AttachmentDesc& other) const
	{
		return std::tie(format, loadOp, clearValue) <
			std::tie(other.format, other.loadOp, other.clearValue);
	}
};

class GfxSubpass
{
public:
	GfxSubpass() = default;
	~GfxSubpass() = default;

	GfxSubpass& AddColorAttachment(AttachmentDesc colorAttchment);
	GfxSubpass& AddInputReference(VkImageLayout layout);
	GfxSubpass& SetDepthStencilAttachment(AttachmentDesc colorAttchment);

private:
	std::vector<AttachmentDesc> colorAttachmentDescs;
	std::vector<VkAttachmentReference> inputReferences;
	AttachmentDesc depthStencilAttachmentDesc;
};

class GfxRenderPass
{

	GfxRenderPass() = default;
	~GfxRenderPass() = default;

	void Initialize(VkDevice newDevice, std::vector<AttachmentDesc> colorAttchments, AttachmentDesc depthAttachment);
	void Destroy();

	void AddSubpass();

private:
	VkDevice device;

	VkRenderPass renderPass;
	std::vector<GfxSubpass> subpasses;
};

