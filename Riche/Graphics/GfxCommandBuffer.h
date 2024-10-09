#pragma once

#include "GfxCore.h"

typedef enum class GfxCommandBufferLevel
{
	PRIMARY = 0,
	SECONDARY = 1
} GfxCommandBufferLevel;

typedef enum class GfxSubpassContents
{
	INLINE = 0,
	SECONDARY_COMMAND_BUFFERS = 1
} GfxSubpassContents;

class GfxCommandBuffer
{
public:
	GfxCommandBuffer() = default;
	~GfxCommandBuffer() = default;

	GfxCommandBuffer& Initialize(VkDevice newDevice, GfxCommandPool* commandPool, GfxCommandBufferLevel bufferLevel = GfxCommandBufferLevel::PRIMARY);
	void Destroy(GfxCommandPool* commandPool);

	bool Begin();
	void CmdBeginRenderPass(GfxRenderPass* renderPass, GfxFrameBuffer* framebuffer, GfxSubpassContents subpassContents = GfxSubpassContents::INLINE, int32_t offsetX = 0, int32_t offsetY = 0, uint32_t width, uint32_t height);
	void CmdBindPipeline(GfxPipeline* pipeline);
	void CmdBindDescriptorSets(uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets);
	void CmdBindPipelineDescriptorSet(const std::vector<VkDescriptorSet>& pipelineDescriptorSet);
	void CmdBindVertexBuffers(const std::vector<GfxBuffer*> vertexBuffers = {});
	void CmdSetViewport();
	void CmdSetScissor();
	void CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
	void CmdNextSubpass();
	void CmdEndRenderPass();
	void End();


	
private:
	VkDevice device;

	GfxCommandBufferLevel level;
	VkCommandBuffer commandBuffer;

	GfxRenderPass* currentRenderPass;
	GfxPipeline* currentPipeline;
	GfxFrameBuffer* currentFrameBuffer;
	uint32_t currentSubpass = 0;
};

