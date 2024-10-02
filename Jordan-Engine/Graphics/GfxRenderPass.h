#pragma once

#include "GfxCore.h"

// VULKAN INFO
// Attachment vs Attachment Reference
// Attachment: 렌더링 동안 사용할 리소스(예, colour, depth, stencil, etc)를 설명하는 구조체.
//			렌더링에 사용될 이미지의 속성을 정의하고 여기에는 format, number of sample, loadOp, storeOp, inital/final layout
// Attachment Reference: 특정 Subpass에서 어떤 Attachment가 사용될지를 지정하는 구조체.
//						Attachment가 이미지의 속성을 설명하는 반면, Ref는 서브패스 내에서 이러한 Attachment가 어떻게 사용(읽기/쓰기)되는지를 지정.

struct GfxAttachmentDesc
{
	VkFormat format;
	VkSampleCountFlagBits samples;
	VkAttachmentLoadOp loadOp;
	VkAttachmentStoreOp storeOp;
	VkAttachmentLoadOp stencilLoadOp;
	VkAttachmentStoreOp stencilStoreOp;
	VkImageLayout initialLayout;
	VkImageLayout finalLayout;
	VkClearValue clearValue;
};

class GfxSubpass
{
public:
	GfxSubpass() = default;
	~GfxSubpass() = default;

	void Initialize(GfxPipelineType type);

	void AddColorAttachmentReference(uint32_t attachment, VkImageLayout layout);
	void AddInputAttachmentReference(uint32_t attachment, VkImageLayout layout);
	void SetDepthStencilAttachmentReference(uint32_t attachment, VkImageLayout layout);

	std::vector<VkAttachmentReference>& GetColorAttachmentReferences();
	std::vector<VkAttachmentReference>& GetInputAttachmentReferences();
	VkAttachmentReference& GetDepthStencilAttachmentReference();
	GfxPipelineType GetPipelineType();

private:
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	std::vector<VkAttachmentReference> inputAttachmentRefs;
	VkAttachmentReference depthStencilAttachmentRef;
	GfxPipelineType type;
};

class GfxRenderPass
{

	GfxRenderPass() = default;
	~GfxRenderPass() = default;

	void Initialize(VkDevice newDevice, std::vector<GfxAttachmentDesc>& newAttachments, std::vector<GfxSubpass>& newSupbasses);
	void Destroy();

	VkRenderPass GetRenderPass();
	std::vector<GfxSubpass>& GetSubpasses();
	std::vector<GfxAttachmentDesc>& GetAttachments();

private:
	VkDevice device;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	std::vector<GfxSubpass> subpasses;
	std::vector<GfxAttachmentDesc> attachments;
};

