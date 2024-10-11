#pragma once

#include <combaseapi.h>
#include "GfxCore.h"

struct Vertex
{
	glm::vec3 pos;	// Vertex pos (x, y, z)
	glm::vec3 col;	// Vertex colour (r, g, b)
	glm::vec2 tex;	// Texture Coords (u, v)
};

class GfxPipeline
{
public:
	GfxPipeline() = default;
	~GfxPipeline() = default;

	void Initialize(VkDevice newDevice);
	void Destroy();

	GfxPipeline& SetVertexShader(const std::string& filename);
	GfxPipeline& SetFragmentShader(const std::string& filename);
	GfxPipeline& SetViewport(VkViewport& newViewport);
	GfxPipeline& SetViewport(float width, float height);
	GfxPipeline& AddInputBindingDescription(uint32_t binding, uint32_t stride);

	GfxPipeline& CreatePipeline(std::vector<VkDescriptorSetLayout>& layouts, VkPushConstantRange pushConstantRange, VkRenderPass renderPass,
		std::vector<VkVertexInputAttributeDescription>& vertexInputAttributeDescs,
		VkPipelineRasterizationStateCreateInfo& rasterizer, VkPipelineMultisampleStateCreateInfo& multisampling,
		VkPipelineColorBlendAttachmentState& blendState, VkPipelineDepthStencilStateCreateInfo& depthStencilState);

	VkPipeline GetVkPipeline();
	VkPipelineLayout GetVkPipelineLayout();
	GfxPipelineType GetPipelineType();
	VkViewport& GetVkViewport();
	VkRect2D& GetVkScissor();

private:
	VkDevice device;

	VkPipeline pipeline;
	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;

	// Shader
	std::string vertextShader;
	std::string fragmentShader;

	// - Vertex Input
	std::vector<VkVertexInputBindingDescription> bindingDescriptions;

	// Viewport & Scissor
	VkViewport viewport;
	VkRect2D scissor;

private:
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
};

// Create 'VkVertexInputAttributeDescription' about Vertex struct. 
inline std::vector<VkVertexInputAttributeDescription> InputAttributeVertexDesc()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	VkVertexInputAttributeDescription attributeDescription;
	// Position Attribute
	attributeDescription.binding = 0;								// Which binding the data is at (should be same as above)
	attributeDescription.location = 0;								// Location in shader where data will be read from
	attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;		// Format the data will take (also helps define size of data)
	attributeDescription.offset = offsetof(Vertex, pos);			// Where this attribute is defined in the data for a single vertex
	attributeDescriptions.push_back(attributeDescription);
	// Texture Attribute
	attributeDescription.binding = 0;
	attributeDescription.location = 2;
	attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescription.offset = offsetof(Vertex, tex);
	attributeDescriptions.push_back(attributeDescription);

	return attributeDescriptions;
}

// -- RASTERIZER --

inline VkPipelineRasterizationStateCreateInfo CullCCWRasterizerCreateInfo()
{
	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;			// Change if fragments beyond near/far planes are clipped (default) or clamped to plane, you can only use this to accept depthBiasClamp of physical device VK_TRUE
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;	// Whether tp discard data and skip rasterizer. Never creates fragments only suitable for pipline without framebuffer output.
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	// How to handle filling points between vertices.
	rasterizerCreateInfo.lineWidth = 1.0f;						// How thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;		// Which face of a tri to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// Winding to determine which side in front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;			// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping

	return rasterizerCreateInfo;
}

inline VkPipelineRasterizationStateCreateInfo CullCWRasterizerCreateInfo()
{
	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;			// Change if fragments beyond near/far planes are clipped (default) or clamped to plane, you can only use this to accept depthBiasClamp of physical device VK_TRUE
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;	// Whether tp discard data and skip rasterizer. Never creates fragments only suitable for pipline without framebuffer output.
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	// How to handle filling points between vertices.
	rasterizerCreateInfo.lineWidth = 1.0f;						// How thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;		// Which face of a tri to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// Winding to determine which side in front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;			// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping

	return rasterizerCreateInfo;
}

inline VkPipelineRasterizationStateCreateInfo WireframeRasterizerCreateInfo()
{
	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;			// Change if fragments beyond near/far planes are clipped (default) or clamped to plane, you can only use this to accept depthBiasClamp of physical device VK_TRUE
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;	// Whether tp discard data and skip rasterizer. Never creates fragments only suitable for pipline without framebuffer output.
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;	// How to handle filling points between vertices.
	rasterizerCreateInfo.lineWidth = 1.0f;						// How thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;		// Which face of a tri to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// Winding to determine which side in front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;			// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping

	return rasterizerCreateInfo;
}

// -- MULTISAMPLING --

inline VkPipelineMultisampleStateCreateInfo NoneMultiSamplingCreateInfo()
{
	// -- MULTISAMPLING --
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;						// Enable multisample shading or not
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;		// Number of samples to use per fragment (e.g. 4xMSAA, 8xMSAA)

	return multisamplingCreateInfo;
}

// -- BLEND STATE --

inline VkPipelineColorBlendAttachmentState AlphaBlendStateInfo()
{
	VkPipelineColorBlendAttachmentState colourState = {};
	colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;					// Colours to apply blending to
	colourState.blendEnable = VK_TRUE;											// Enable Blending

	// Blending uses equation: (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
	colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;				// if it is 0.3
	colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		// this is (1.0 - 0.3)
	colourState.colorBlendOp = VK_BLEND_OP_ADD;
	// Summarised: (SRC_ALPHA * new colour) + (MINUS_SRC_ALPHA * old colour) == (alpha * new ) + (1 - alpha * old)

	colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colourState.alphaBlendOp = VK_BLEND_OP_ADD;
	// Summarised: (1 * new alpha) + (0 * old alpha) == new alpha
}

inline VkPipelineColorBlendAttachmentState AdditiveBlendStateInfo()
{
	VkPipelineColorBlendAttachmentState colourState = {};
	colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;					// Colours to apply blending to
	colourState.blendEnable = VK_TRUE;											// Enable Blending

	// Blending uses equation: (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
	colourState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;				// if it is 0.3
	colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;		// this is (1.0 - 0.3)
	colourState.colorBlendOp = VK_BLEND_OP_ADD;
	// Summarised: (SRC_ALPHA * new colour) + (MINUS_SRC_ALPHA * old colour) == (alpha * new ) + (1 - alpha * old)

	colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colourState.alphaBlendOp = VK_BLEND_OP_ADD;
	// Summarised: (1 * new alpha) + (0 * old alpha) == new alpha
}

// -- DEPTH STENCIL STATE --

inline VkPipelineDepthStencilStateCreateInfo DefaultDepthStencilStateCreateInfo()
{
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;				// Enable checking depth to determine fragment wrtie
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;				// Enable writing to depth buffer (to replace old values)
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;		// Comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		// Depth Bounds Test: Does the depth value exist between two bounds, 즉 픽셀의 깊이 값이 특정 범위 안에 있는지를 체크하는 검사
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;			// Enable Stencil Test

	return depthStencilCreateInfo;
}

inline VkPipelineDepthStencilStateCreateInfo NoneDepthStencilStateCreateInfo()
{
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_FALSE;				// Enable checking depth to determine fragment wrtie
	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;				// Enable writing to depth buffer (to replace old values)
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;		// Comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		// Depth Bounds Test: Does the depth value exist between two bounds, 즉 픽셀의 깊이 값이 특정 범위 안에 있는지를 체크하는 검사
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;			// Enable Stencil Test

	return depthStencilCreateInfo;
}


