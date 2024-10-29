#include "GfxPipeline.h"

void GfxPipeline::Initialize(VkDevice newDevice)
{
	device = newDevice;
}

void GfxPipeline::Destroy()
{
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
}

GfxPipeline& GfxPipeline::SetVertexShader(const std::string& filename)
{
	vertextShader = filename;
	return *this;
}

GfxPipeline& GfxPipeline::SetFragmentShader(const std::string& filename)
{
	fragmentShader = filename;
	return *this;
}

GfxPipeline& GfxPipeline::SetViewport(VkViewport& newViewport)
{
	viewport = newViewport;

	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

	return *this;
}

GfxPipeline& GfxPipeline::SetViewport(float width, float height)
{
	VkViewport newViewport = {};
	newViewport.x = 0.0f;
	newViewport.y = 0.0f;
	newViewport.width = width;
	newViewport.height = height;
	newViewport.minDepth = 0.0f;
	newViewport.maxDepth = 1.0f;

	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

	return *this;
}

GfxPipeline& GfxPipeline::AddInputBindingDescription(uint32_t binding, uint32_t stride)
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = binding;										// Can bind multiple streams of data, this defines which one
	bindingDescription.stride = stride;											// size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;					// How to move between data after each vertex

	bindingDescriptions.push_back(bindingDescription);

	return *this;
}

GfxPipeline& GfxPipeline::SetVertexInputState(std::vector<VkVertexInputAttributeDescription> newVertexInputAttributeDescs)
{
	// -- VERTEX INPUT --
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();					// List of Vertex binding descriptions (data spacing / stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(newVertexInputAttributeDescs.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = newVertexInputAttributeDescs.data();		// List of Vertex Attribute Descripitions ( data format and where to bind from)

	return *this;
}

GfxPipeline& GfxPipeline::SetRasterizationState(VkPipelineRasterizationStateCreateInfo newRasterizer)
{
	rasterizationState = newRasterizer;
	return *this;
}

GfxPipeline& GfxPipeline::SetMultiSampleState(VkPipelineMultisampleStateCreateInfo newMultisampling)
{
	multisampleState = newMultisampling;
	return *this;
}

GfxPipeline& GfxPipeline::SetColorBlendAttachmentState(VkPipelineColorBlendAttachmentState newBlendState)
{
	VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
	colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlendingCreateInfo.logicOpEnable = VK_FALSE;							// Alternative to calculations is to use logical operations
	colourBlendingCreateInfo.attachmentCount = 1;
	colourBlendingCreateInfo.pAttachments = &newBlendState;
	
	return *this;
}

GfxPipeline& GfxPipeline::SetDepthStencilState(VkPipelineDepthStencilStateCreateInfo newDepthStencilState)
{
	depthStencilState = newDepthStencilState;
	return *this;
}

GfxPipeline& GfxPipeline::CreatePipeline(std::vector<VkDescriptorSetLayout>& layouts, VkPushConstantRange pushConstantRange, VkRenderPass renderPass)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	// Create pipeline layout
	VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create Pipeline Layout!");
	}

	// Shader Moudule
	auto vertexShaderCode = readFile(vertextShader);
	auto fragmentShaderCode = readFile(fragmentShader);

	VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderCode);

	// -- SHADER STAGE CREATION INFOMATION --
	// Vertex Stage Creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;						// Shader stage name
	vertexShaderCreateInfo.module = vertexShaderModule;								// Shader moudle to be used by stage
	vertexShaderCreateInfo.pName = "main";											// Entry point in to shader
	// Fragment Stage Creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;					// Shader stage name
	fragmentShaderCreateInfo.module = fragmentShaderModule;							// Shader moudle to be used by stage
	fragmentShaderCreateInfo.pName = "main";										// Entry point in to shader

	// Graphics pipeline creation info requires array of shader stage creates
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// -- INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// List versus Strip: ���ӵ� ��(Strip), �� �� ��� (List)
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		// Primitive type to assemble vertices
	inputAssembly.primitiveRestartEnable = VK_FALSE;					// Allow overriding of "strip" topology to start new primitives

	// -- VIEWPORT --
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;										// Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;								// List of shader stages
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;			// All the fixed function pipeline states
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.layout = pipelineLayout;								// Pipeline Laytout pipeline should use
	pipelineCreateInfo.renderPass = renderPass;								// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;											// Subpass of render pass to use with pipeline

	// Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	// Existing pipline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;				// or index of pipeline being created to derive from (in case createing multiple at once)

	// Create Grahpics Pipeline
	result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create Graphics Pipeline!");
	}

	return *this;
}

VkPipeline GfxPipeline::GetVkPipeline()
{
	return pipeline;
}

VkPipelineLayout GfxPipeline::GetVkPipelineLayout()
{
	return pipelineLayout;
}

GfxPipelineType GfxPipeline::GetPipelineType()
{
	return GfxPipelineType::Graphics;
}

VkViewport& GfxPipeline::GetVkViewport()
{
	return viewport;
}

VkRect2D& GfxPipeline::GetVkScissor()
{
	return scissor;
}

VkShaderModule GfxPipeline::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to create a shader moudle");
	}

	return shaderModule;
}
