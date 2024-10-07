#include "GfxPipeline.h"

void GfxGraphicsPipeline::Initialize(VkDevice newDevice)
{
	device = newDevice;
}

GfxGraphicsPipeline& GfxGraphicsPipeline::SetVertexShader(const std::string& filename)
{
	vertextShader = filename;
	return *this;
}

GfxGraphicsPipeline& GfxGraphicsPipeline::SetFragmentShader(const std::string& filename)
{
	fragmentShader = filename;
	return *this;
}

GfxGraphicsPipeline& GfxGraphicsPipeline::SetViewport(VkViewport& newViewport)
{
	viewport = newViewport;

	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

	return *this;
}

GfxGraphicsPipeline& GfxGraphicsPipeline::SetViewport(float width, float height)
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

GfxGraphicsPipeline& GfxGraphicsPipeline::AddInputBindingDescription(uint32_t binding, uint32_t stride)
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = binding;										// Can bind multiple streams of data, this defines which one
	bindingDescription.stride = stride;											// size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;					// How to move between data after each vertex

	bindingDescriptions.push_back(bindingDescription);

	return *this;
}

GfxGraphicsPipeline& GfxGraphicsPipeline::CreatePipeline(std::vector<VkDescriptorSetLayout>& layouts, VkPushConstantRange pushConstantRange, VkRenderPass renderPass,
	std::vector<VkVertexInputAttributeDescription>& vertexInputAttributeDescs,
	VkPipelineRasterizationStateCreateInfo& rasterizer, VkPipelineMultisampleStateCreateInfo& multisampling, 
	VkPipelineColorBlendAttachmentState& blendState, VkPipelineDepthStencilStateCreateInfo& depthStencilState)
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

	// -- VERTEX INPUT --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();					// List of Vertex binding descriptions (data spacing / stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescs.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescs.data();		// List of Vertex Attribute Descripitions ( data format and where to bind from)

	// -- INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// List versus Strip: ¿¬¼ÓµÈ Á¡(Strip), µü µü ²÷¾î¼­ (List)
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		// Primitive type to assemble vertices
	inputAssembly.primitiveRestartEnable = VK_FALSE;					// Allow overriding of "strip" topology to start new primitives

	// -- VIEWPORT --
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	// -- BLEND STATE CREATE INFO --
	VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
	colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlendingCreateInfo.logicOpEnable = VK_FALSE;							// Alternative to calculations is to use logical operations
	colourBlendingCreateInfo.attachmentCount = 1;
	colourBlendingCreateInfo.pAttachments = &blendState;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;										// Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;								// List of shader stages
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;			// All the fixed function pipeline states
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState = &multisampling;
	pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.layout = pipelineLayout;								// Pipeline Laytout pipeline should use
	pipelineCreateInfo.renderPass = renderPass;								// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;											// Subpass of render pass to use with pipeline

	// Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	// Existing pipline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;				// or index of pipeline being created to derive from (in case createing multiple at once)

	// Create Grahpics Pipeline
	result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create Graphics Pipeline!");
	}

	return *this;
}

VkShaderModule GfxGraphicsPipeline::CreateShaderModule(const std::vector<char>& code)
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
