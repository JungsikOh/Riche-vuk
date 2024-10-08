#include "GfxCommandBuffer.h"

GfxCommandBuffer& GfxCommandBuffer::Initialize(VkDevice newDevice, 
	GfxCommandPool* commandPool, GfxCommandBufferLevel bufferLevel = GfxCommandBufferLevel::PRIMARY)
{
	device = newDevice;

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = commandPool->GetVkCommandPool();			// �ش� ť �йи��� ť������ Ŀ�ǵ� ť ������ ���డ���ϴ�.
	cbAllocInfo.level = bufferLevel == GfxCommandBufferLevel::PRIMARY ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;				// VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to queue. cant be called by other buffers
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via 'vkCmdExecuteCommands' when recording commands in primary buffer.
	cbAllocInfo.commandBufferCount = 1;

	// Allocate command buffers and place handles in array of buffers
	VkResult result = vkAllocateCommandBuffers(device, &cbAllocInfo, &commandBuffer);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to allocate Command Buffers!");
	}

	return *this;
}

void GfxCommandBuffer::Destroy(GfxCommandPool* commandPool)
{
	vkFreeCommandBuffers(device, commandPool->GetVkCommandPool(), 1, &commandBuffer);
}

bool GfxCommandBuffer::Begin()
{
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;		// Buffer can be resubmitted when it has already been submitted and is awaiting execution

	VkResult result = vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to start recording a Command Buffer!");
	}
}
void GfxCommandBuffer::CmdBeginRenderPass(GfxRenderPass* renderPass, GfxFrameBuffer* framebuffer, 
	GfxSubpassContents subpassContents = GfxSubpassContents::INLINE, 
	int32_t offsetX = 0, int32_t offsetY = 0, uint32_t width, uint32_t height)
{
	currentRenderPass = renderPass;
	currentFrameBuffer = framebuffer;

	std::vector<GfxAttachmentDesc> attachments = currentRenderPass->GetAttachments();
	std::vector<VkClearValue> clearValues = {};

	for (size_t i = 0; i < attachments.size(); ++i)
	{
		VkFormat format = attachments[i].format;
		if (format == VK_FORMAT_D24_UNORM_S8_UINT) {
			VkClearValue cv = {};
			cv.depthStencil.depth = 1.0f;
			cv.depthStencil.stencil = 0.0f;

			clearValues.push_back(cv);
		}
		else if (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R16G16B16A16_UNORM)
		{
			VkClearValue cv = {};
			cv.color = { 0, 0, 0, 0 };

			clearValues.push_back(cv);
		}
	}

	// Information about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = currentRenderPass->GetRenderPass();							// Render Pass to begin
	renderPassBeginInfo.renderArea.offset = { offsetX, offsetY };										// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = { width, height };										// Size of region to run render pass on (starting at offset)

	renderPassBeginInfo.pClearValues = clearValues.data();								// List of clear values
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = currentFrameBuffer->GetVkFrameBuffer();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // ���� �н��� ������ ���� ���� ���ۿ� ����ϴ� ���� �ǹ�
}

void GfxCommandBuffer::CmdBindPipeline(GfxPipeline* pipeline)
{
	currentPipeline = pipeline;

	vkCmdBindPipeline(commandBuffer, currentPipeline->GetPipelineType() == GfxPipelineType::Graphics ?
		VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE, currentPipeline->GetVkPipeline());
}

void GfxCommandBuffer::CmdBindDescriptorSets(uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets)
{
	VkPipelineBindPoint bindPoint = currentPipeline->GetPipelineType() == GfxPipelineType::Graphics ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;
	
	vkCmdBindDescriptorSets(commandBuffer, bindPoint,
		currentPipeline->GetVkPipelineLayout(), 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

void GfxCommandBuffer::CmdBindPipelineDescriptorSet(const std::vector<VkDescriptorSet>& pipelineDescriptorSet)
{
	VkPipelineBindPoint bindPoint = currentPipeline->GetPipelineType() == GfxPipelineType::Graphics ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;

	vkCmdBindDescriptorSets(commandBuffer, bindPoint,
		currentPipeline->GetVkPipelineLayout(), 0, static_cast<uint32_t>(pipelineDescriptorSet.size()), pipelineDescriptorSet.data(), 0, nullptr);
}

void GfxCommandBuffer::CmdBindVertexBuffers(const std::vector<GfxBuffer*> vertexBuffers = {})
{
	std::vector<VkBuffer> vertexVkBuffers;
	std::vector<VkDeviceSize> temp;

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
}

void CmdSetViewport(const VkViewport viewports = {});
void CmdSetScissor(const VkRect2D scissors = {});
void CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void CmdNextSubpass();
void CmdEndRenderPass();