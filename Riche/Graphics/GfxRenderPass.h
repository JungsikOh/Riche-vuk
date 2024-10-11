#pragma once

#include "GfxCore.h"

// VULKAN INFO
// Attachment vs Attachment Reference
// Attachment: ������ ���� ����� ���ҽ�(��, colour, depth, stencil, etc)�� �����ϴ� ����ü.
//			�������� ���� �̹����� �Ӽ��� �����ϰ� ���⿡�� format, number of sample, loadOp, storeOp, inital/final layout
// Attachment Reference: Ư�� Subpass���� � Attachment�� �������� �����ϴ� ����ü.
//						Attachment�� �̹����� �Ӽ��� �����ϴ� �ݸ�, Ref�� �����н� ������ �̷��� Attachment�� ��� ���(�б�/����)�Ǵ����� ����.

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

	GfxSubpass& AddColorAttachmentReference(uint32_t attachment, VkImageLayout layout);
	GfxSubpass& AddInputAttachmentReference(uint32_t attachment, VkImageLayout layout);
	GfxSubpass& SetDepthStencilAttachmentReference(uint32_t attachment, VkImageLayout layout);

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
public:
	GfxRenderPass();
	~GfxRenderPass();

	void Initialize(VkDevice newDevice, std::vector<GfxAttachmentDesc>& newAttachments, std::vector<GfxSubpass>& newSupbasses);
	void Destroy();

	VkRenderPass GetRenderPass();
	std::vector<GfxSubpass>& GetSubpasses();
	std::vector<GfxAttachmentDesc>& GetAttachments();

	VkDevice GetDevice() const;

private:
	VkDevice device;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	std::vector<GfxSubpass> subpasses;
	std::vector<GfxAttachmentDesc> attachments;
};

