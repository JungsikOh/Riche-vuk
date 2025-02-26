#include "CullingRenderPass.h"

#include "Camera.h"

CullingRenderPass::CullingRenderPass(VkDevice device, VkPhysicalDevice physicalDevice) {}

void CullingRenderPass::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool,
                                   Camera* camera, Editor* editor, const uint32_t width, const uint32_t height) {
  m_pDevice = device;
  m_pPhyscialDevice = physicalDevice;
  m_pGraphicsQueue = queue;

  m_width = width;
  m_height = height;

  m_pCamera = camera;

  m_pGraphicsCommandPool = commandPool;

  /*
   * Create Resources
   */
  CreateRenderPass();
  CreateFramebuffers();
  CreateBuffers();

  CreateDesrciptorSets();
  CreatePushConstantRange();

  CreatePipelineLayouts();
  CreatePipelines();

  SetupQueryPool();

  /*
   * Synchronization + Command Buffer
   */
  CreateSemaphores();
  CreateCommandBuffers();
}

void CullingRenderPass::Cleanup() {
  for (int i = 0; i < MAX_FRAME_DRAWS; ++i) {
    vkDestroySemaphore(m_pDevice, m_renderAvailable[i], nullptr);
    vkDestroyFence(m_pDevice, m_fence[i], nullptr);
  }
  vkFreeCommandBuffers(m_pDevice, m_pGraphicsCommandPool, MAX_FRAME_DRAWS, m_commandBuffers.data());

  vkDestroyPipelineLayout(m_pDevice, m_graphicsPipelineLayout, nullptr);

  // DepthOnly
  vkDestroyFramebuffer(m_pDevice, m_depthOnlyFramebuffer, nullptr);
  vkDestroyImageView(m_pDevice, m_depthOnlyBufferImage.imageView, nullptr);
  vkDestroyImage(m_pDevice, m_depthOnlyBufferImage.image, nullptr);
  vkFreeMemory(m_pDevice, m_depthOnlyBufferImage.memory, nullptr);

  vkDestroyPipeline(m_pDevice, m_depthGraphicePipeline, nullptr);
  vkDestroyRenderPass(m_pDevice, m_depthRenderPass, nullptr);

  vkDestroyQueryPool(m_pDevice, m_occlusionQueryPool, nullptr);
}

void CullingRenderPass::Update(uint32_t imageIndex) {
  void* pData = nullptr;

  if (g_RenderSetting.isOcclusionCulling) {
    int accumulatedIndex = 0;
    std::vector<VkDrawIndexedIndirectCommand> commands{};
    for (auto& batch : g_BatchManager.m_miniBatchList) {
      for (int i = 0; i < batch.m_drawIndexedCommands.size(); ++i) {
        if (m_passedSamples[accumulatedIndex++] != 0) {
          batch.m_drawIndexedCommands[i].instanceCount = 1;
        } else {
          batch.m_drawIndexedCommands[i].instanceCount = 0;
        }
        // if(accumulatedIndex != m_passedSamples.size()) std::cout << m_passedSamples[accumulatedIndex] << ", ";
      }
      commands.insert(commands.end(), batch.m_drawIndexedCommands.begin(), batch.m_drawIndexedCommands.end());
    }
    vkMapMemory(m_pDevice, g_BatchManager.m_indirectDrawCommandBuffer.memory, 0, g_BatchManager.m_indirectDrawCommandBuffer.size, 0,
                &pData);
    memcpy(pData, commands.data(), g_BatchManager.m_indirectDrawCommandBuffer.size);
    vkUnmapMemory(m_pDevice, g_BatchManager.m_indirectDrawCommandBuffer.memory);
  }

  /*
   * Debug
   */

  // Number of Rendering Object
  void* mappedMemory = nullptr;
  {
    vkMapMemory(m_pDevice, g_BatchManager.m_indirectDrawCommandBuffer.memory, 0, g_BatchManager.m_indirectDrawCommandBuffer.size, 0,
                &mappedMemory);
    VkDrawIndexedIndirectCommand* drawCommands = reinterpret_cast<VkDrawIndexedIndirectCommand*>(mappedMemory);
    uint64_t commandCount =
        static_cast<uint64_t>(g_BatchManager.m_indirectDrawCommandBuffer.size / sizeof(VkDrawIndexedIndirectCommand));
    std::vector<VkDrawIndexedIndirectCommand> commandVector(drawCommands, drawCommands + commandCount);
    uint64_t instantCount = 0;
    for (int i = 0; i < commandVector.size(); ++i) {
      instantCount += commandVector[i].instanceCount == 0 ? 0 : 1;
    }
    g_RenderSetting.afterViewCullingRenderingNum = instantCount;
    vkUnmapMemory(m_pDevice, g_BatchManager.m_indirectDrawCommandBuffer.memory);
  }
}

void CullingRenderPass::Draw(uint32_t imageIndex, VkFence fence, VkSemaphore renderAvailable) {
  RecordCommands(imageIndex);

  // RenderPass 1.
  VkSubmitInfo basicSubmitInfo = {};
  basicSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  basicSubmitInfo.waitSemaphoreCount = 1;  // Number of semaphores to wait on
  basicSubmitInfo.pWaitSemaphores = &renderAvailable;

  VkPipelineStageFlags basicWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
  basicSubmitInfo.pWaitDstStageMask = basicWaitStages;                 // Stages to check semaphores at
  basicSubmitInfo.commandBufferCount = 1;                              // Number of command buffers to submit
  basicSubmitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];     // Command buffer to submit
  basicSubmitInfo.signalSemaphoreCount = 1;                            // Number of semaphores to signal
  basicSubmitInfo.pSignalSemaphores = &m_renderAvailable[imageIndex];  // Semaphores to signal when command buffer finishes
  // Command buffer가 실행을 완료하면, Signaled 상태가 될 semaphore 배열.

  VK_CHECK(vkQueueSubmit(m_pGraphicsQueue, 1, &basicSubmitInfo, nullptr));

  if (g_RenderSetting.isOcclusionCulling) {
    GetQueryResults();
  }
}

void CullingRenderPass::CreateRenderPass() { CreateDepthRenderPass(); }

void CullingRenderPass::CreateDepthRenderPass() {
  // Array of Subpasses
  std::array<VkSubpassDescription, 1> subpasses{};

  // ATTACHMENTS
  // SUBPASS 1 ATTACHMENTS (INPUT ATTACHMEMNTS)
  VkAttachmentDescription depthStencilAttachment = {};
  depthStencilAttachment.format = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT}, VK_IMAGE_TILING_OPTIMAL,
                                                                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentRef = {};
  depthStencilAttachmentRef.attachment = 0;
  depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // Set up Subpass 1
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpasses[0].colorAttachmentCount = 0;
  subpasses[0].pColorAttachments = VK_NULL_HANDLE;
  subpasses[0].pDepthStencilAttachment = &depthStencilAttachmentRef;

  // Need to determine when layout transitions occur using subpass dependencies
  std::array<VkSubpassDependency, 2> subpassDependencies;

  // Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  // Transition must happen after ..
  subpassDependencies[0].srcSubpass =
      VK_SUBPASS_EXTERNAL;  // 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
  subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // Pipeline stage
  subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;            // Stage access mask (memory access)
  // But most happen before ..
  subpassDependencies[0].dstSubpass = 0;
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dependencyFlags = 0;

  // Subpass 1 layout (colour/depth) to Externel(image/image)
  subpassDependencies[1].srcSubpass = 0;
  subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  subpassDependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;  // We do not link swapchain. So, dstStageMask is to be SHADER_BIT.
  subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpassDependencies[1].dependencyFlags = 0;

  std::array<VkAttachmentDescription, 1> renderPassAttachments = {depthStencilAttachment};

  // Create info for Render Pass
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
  renderPassCreateInfo.pAttachments = renderPassAttachments.data();
  renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
  renderPassCreateInfo.pSubpasses = subpasses.data();
  renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
  renderPassCreateInfo.pDependencies = subpassDependencies.data();

  VK_CHECK(vkCreateRenderPass(m_pDevice, &renderPassCreateInfo, nullptr, &m_depthRenderPass));
}

void CullingRenderPass::CreateFramebuffers() { CreateDepthFramebuffer(); }

void CullingRenderPass::CreateDepthFramebuffer() {
  VkFormat depthImageFormat = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT}, VK_IMAGE_TILING_OPTIMAL,
                                                             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_depthOnlyBufferImage.memory,
                         &m_depthOnlyBufferImage.image, depthImageFormat,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkUtils::CreateImageView(m_pDevice, m_depthOnlyBufferImage.image, &m_depthOnlyBufferImage.imageView, depthImageFormat,
                           VK_IMAGE_ASPECT_DEPTH_BIT);

  std::array<VkImageView, 1> attachments = {m_depthOnlyBufferImage.imageView};

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.renderPass = m_depthRenderPass;  // Render pass layout the framebuffer will be used with
  framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferCreateInfo.pAttachments = attachments.data();  // List of attachments (1:1 with render pass)
  framebufferCreateInfo.width = m_width;                    // framebuffer width
  framebufferCreateInfo.height = m_height;                  // framebuffer height
  framebufferCreateInfo.layers = 1;                         // framebuffer layers

  VK_CHECK(vkCreateFramebuffer(m_pDevice, &framebufferCreateInfo, nullptr, &m_depthOnlyFramebuffer));
}

void CullingRenderPass::CreatePipelineLayouts() {
  // -- PIPELINE LAYOUT (It's like Root signature in D3D12) --

  // Graphics Pipeline
  {
    std::array<VkDescriptorSetLayout, 2> setGraphicsLayouts = {
        g_DescriptorManager.GetVkDescriptorSetLayout("ViewProjection_ALL0"),
        g_DescriptorManager.GetVkDescriptorSetLayout("BATCH_ALL0"),
    };

    VkPipelineLayoutCreateInfo grahpicsPipelineLayoutCreateInfo = {};
    grahpicsPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    grahpicsPipelineLayoutCreateInfo.setLayoutCount = setGraphicsLayouts.size();
    grahpicsPipelineLayoutCreateInfo.pSetLayouts = setGraphicsLayouts.data();
    grahpicsPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    grahpicsPipelineLayoutCreateInfo.pPushConstantRanges = &m_debugPushConstant;

    VK_CHECK(vkCreatePipelineLayout(m_pDevice, &grahpicsPipelineLayoutCreateInfo, nullptr, &m_graphicsPipelineLayout));
  }
}

void CullingRenderPass::CreatePipelines() {
  CreateDepthGraphicsPipeline();
}

void CullingRenderPass::CreateDepthGraphicsPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/DepthOnlyVS.spv");

  // Build Shaders
  VkShaderModule vertexShaderModule = VkUtils::CreateShaderModule(m_pDevice, vertexShaderCode);

  // Set new shaders
  // Vertex Stage Creation information
  VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
  vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // Shader stage name
  vertexShaderCreateInfo.module = vertexShaderModule;         // Shader moudle to be used by stage
  vertexShaderCreateInfo.pName = "main";                      // Entry point in to shader

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo};

  // -- VERTEX INPUT --
  // -- Create Pipeline --
  // How the data for a single vertex (including info suach as position, colour, textuer coords, normals, etc) is as a whole.
  VkVertexInputBindingDescription bindingDescription = {};
  bindingDescription.binding = 0;                                          // Can bind multiple streams of data, this defines which one
  bindingDescription.stride = static_cast<uint32_t>(sizeof(BasicVertex));  // size of a single vertex object
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;              // How to move between data after each vertex
  // VK_VERTEX_INPUT_RATE_VERTEX		: Move on to the next vertex
  // VK_VERTEX_INPUT_RATE_INSTANCE	: Move to a vertex for the next instance

  // How the data for an attribute is defined whithin a vertex
  std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;
  // Position Attribute
  attributeDescriptions[0].binding = 0;                          // Which binding the data is at (should be same as above)
  attributeDescriptions[0].location = 0;                         // Location in shader where data will be read from
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // Format the data will take (also helps define size of data)
  attributeDescriptions[0].offset = offsetof(BasicVertex, pos);  // Where this attribute is defined in the data for a single vertex.
  // Colour Attribute
  attributeDescriptions[1].binding = 0;                             // Which binding the data is at (should be same as above)
  attributeDescriptions[1].location = 1;                            // Location in shader where data will be read from
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;     // Format the data will take (also helps define size of data)
  attributeDescriptions[1].offset = offsetof(BasicVertex, normal);  // Where this attribute is defined in the data for a single vertex.
  // Texture Attribute
  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(BasicVertex, tex);
  // -- VERTEX INPUT --
  VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
  vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
  vertexInputCreateInfo.pVertexBindingDescriptions =
      &bindingDescription;  // List of Vertex binding descriptions (data spacing / stride information)
  vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputCreateInfo.pVertexAttributeDescriptions =
      attributeDescriptions.data();  // List of Vertex Attribute Descripitions ( data format and where to bind from)

  // -- INPUT ASSEMBLY --
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  // List versus Strip: 연속된 점(Strip), 딱 딱 끊어서 (List)
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // Primitive type to assemble vertices
  inputAssembly.primitiveRestartEnable = VK_FALSE;               // Allow overriding of "strip" topology to start new primitives

  // -- VIPEPORT & SCISSOR --
  // Create a viewport info struct
  VkViewport viewport = {};
  viewport.x = 0.0f;                  // x start coordinate
  viewport.y = 0.0f;                  // y start coordinate
  viewport.width = (float)m_width;    // width of viewport
  viewport.height = (float)m_height;  // height of viewport
  viewport.minDepth = 0.0f;           // min framebuffer depth
  viewport.maxDepth = 1.0f;           // max framebuffer depth

  // Create a scissor info struct
  VkRect2D scissor = {};
  scissor.offset = {0, 0};               // offset to use region from
  scissor.extent = {m_width, m_height};  // extent to describe region to use, starting at offset

  VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
  viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateCreateInfo.viewportCount = 1;
  viewportStateCreateInfo.pViewports = &viewport;
  viewportStateCreateInfo.scissorCount = 1;
  viewportStateCreateInfo.pScissors = &scissor;

  // -- RASTERIZER --
  VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
  rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizerCreateInfo.depthClampEnable =
      VK_FALSE;  // Change if fragments beyond near/far planes are clipped (default) or clamped to
                 // plane, you can only use this to accept depthBiasClamp of physical device VK_TRUE
  rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;  // Whether tp discard data and skip rasterizer. Never creates fragments
                                                            // only suitable for pipline without framebuffer output.
  rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;  // How to handle filling points between vertices.
  rasterizerCreateInfo.lineWidth = 1.0f;                    // How thick lines should be when drawn
  rasterizerCreateInfo.cullMode = VK_CULL_MODE_NONE;        // Which face of a tri to cull
  rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // Winding to determine which side in front
  rasterizerCreateInfo.depthBiasEnable =
      VK_FALSE;  // Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping

  // -- MULTISAMPLING --
  VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
  multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;                // Enable multisample shading or not
  multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // Number of samples to use per fragment (e.g. 4xMSAA, 8xMSAA)

  // -- BLENDING --
  // Blending decides how to blend a new colour being written to a fragment, with the old value
  // Blend Attacment State (how blending is handled)
  VkPipelineColorBlendAttachmentState colourState = {};
  colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;  // Colours to apply blending to
  colourState.blendEnable = VK_FALSE;                     // Enable Blending

  // Blending uses equation: (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
  colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;            // if it is 0.3
  colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // this is (1.0 - 0.3)
  colourState.colorBlendOp = VK_BLEND_OP_ADD;
  // Summarised: (SRC_ALPHA * new colour) + (MINUS_SRC_ALPHA * old colour) == (alpha * new ) + (1 - alpha * old)

  colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colourState.alphaBlendOp = VK_BLEND_OP_ADD;
  // Summarised: (1 * new alpha) + (0 * old alpha) == new alpha

  VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
  colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colourBlendingCreateInfo.logicOpEnable = VK_FALSE;  // Alternative to calculations is to use logical operations
  colourBlendingCreateInfo.attachmentCount = 0;
  colourBlendingCreateInfo.pAttachments = VK_NULL_HANDLE;

  //
  // Use Graphics Pipeline Layout
  //

  // Don't want to write to depth buffer
  // -- DEPTH STENCIL TESTING --
  VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
  depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilCreateInfo.depthTestEnable = VK_TRUE;            // Enable checking depth to determine fragment wrtie
  depthStencilCreateInfo.depthWriteEnable = VK_TRUE;           // Enable writing to depth buffer (to replace old values)
  depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;  // Comparison operation that allows an overwrite (is in front)
  depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;     // Depth Bounds Test: Does the depth value exist between two bounds, 즉
                                                            // 픽셀의 깊이 값이 특정 범위 안에 있는지를 체크하는 검사
  depthStencilCreateInfo.stencilTestEnable = VK_FALSE;  // Enable Stencil Test

  // -- GRAPHICS PIPELINE CREATION --
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = 1;                              // Number of shader stages
  pipelineCreateInfo.pStages = shaderStages;                      // List of shader stages
  pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;  // All the fixed function pipeline states
  pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
  pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
  pipelineCreateInfo.pDynamicState = nullptr;
  pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
  pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
  pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
  pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
  pipelineCreateInfo.layout = m_graphicsPipelineLayout;  // Pipeline Laytout pipeline should use
  pipelineCreateInfo.renderPass = m_depthRenderPass;     // Render pass description the pipeline is compatible with
  pipelineCreateInfo.subpass = 0;                        // Subpass of render pass to use with pipeline

  // Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;  // Existing pipline to derive from
  pipelineCreateInfo.basePipelineIndex = -1;  // or index of pipeline being created to derive from (in case createing multiple at once)

  VK_CHECK(vkCreateGraphicsPipelines(m_pDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_depthGraphicePipeline));

  // Destroy second shader modules
  vkDestroyShaderModule(m_pDevice, vertexShaderModule, nullptr);
}

void CullingRenderPass::SetupQueryPool() {
  m_passedSamples.resize(g_BatchManager.m_meshes.size());

  VkQueryPoolCreateInfo queryPoolInfo = {};
  queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  queryPoolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
  queryPoolInfo.queryCount = static_cast<uint32_t>(g_BatchManager.m_meshes.size());
  VK_CHECK(vkCreateQueryPool(m_pDevice, &queryPoolInfo, nullptr, &m_occlusionQueryPool));
}

void CullingRenderPass::GetQueryResults() {
  // We use vkGetQueryResults to copy the results into a host visible buffer
  vkGetQueryPoolResults(m_pDevice, m_occlusionQueryPool, 0, static_cast<uint32_t>(g_BatchManager.m_meshes.size()),
                        m_passedSamples.size() * sizeof(uint64_t), m_passedSamples.data(), sizeof(uint64_t),
                        // Store results a 64 bit values and wait until the results have been finished
                        // If you don't want to wait, you can use VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
                        // which also returns the state of the result (ready) in the result
                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

void CullingRenderPass::CreateBuffers() {
  CreateUniformBuffers();
  CreateShaderStorageBuffers();
}

void CullingRenderPass::CreateShaderStorageBuffers() {
  m_frustumPlanes = CalculateFrustumPlanes(m_pCamera->ViewProj());
}

void CullingRenderPass::CreateUniformBuffers() {}

void CullingRenderPass::CreateDesrciptorSets() {
}

void CullingRenderPass::CreatePushConstantRange() {
  m_debugPushConstant.stageFlags = VK_SHADER_STAGE_ALL;  // Shader stage push constant will go to
  m_debugPushConstant.offset = 0;                        // Offset into given data to pass to push constant
  m_debugPushConstant.size = sizeof(ShaderSetting);      // Size of Data Being Passed
}

void CullingRenderPass::CreateSemaphores() {
  m_fence.resize(MAX_FRAME_DRAWS);
  m_renderAvailable.resize(MAX_FRAME_DRAWS);

  // Semaphore creataion information
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < MAX_FRAME_DRAWS; ++i) {
    VK_CHECK(vkCreateFence(m_pDevice, &fenceCreateInfo, nullptr, &m_fence[i]));
    VK_CHECK(vkCreateSemaphore(m_pDevice, &semaphoreCreateInfo, nullptr, &m_renderAvailable[i]));
  }
}

void CullingRenderPass::CreateCommandBuffers() {
  //
  // Triple Command Buffer
  //
  m_commandBuffers.resize(MAX_FRAME_DRAWS);

  VkCommandBufferAllocateInfo cbAllocInfo = {};
  cbAllocInfo = {};
  cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAllocInfo.commandPool = m_pGraphicsCommandPool;  // 해당 큐 패밀리의 큐에서만 커맨드 큐 동작이 실행가능하다.
  cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to
                                                        // queue. cant be called by other buffers
  // VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via 'vkCmdExecuteCommands'
  // when recording commands in primary buffer.
  cbAllocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

  // Allocate command buffers and place handles in array of buffers
  VK_CHECK(vkAllocateCommandBuffers(m_pDevice, &cbAllocInfo, m_commandBuffers.data()));
}

void CullingRenderPass::RecordCommands(uint32_t currentImage) {
  g_RenderSetting.beforeCullingRenderingNum = 0;
  for (auto& miniBatch : g_BatchManager.m_miniBatchList) {
    g_RenderSetting.beforeCullingRenderingNum += miniBatch.m_drawIndexedCommands.size();
  }

  // information about how to begin each command buffer
  VkCommandBufferBeginInfo bufferBeginInfo = {};
  bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;  // Buffer can be resubmitted when it has already been
                                                                         // submitted and is awaiting executi

  // Start recording commands to command buffer!
  VK_CHECK(vkBeginCommandBuffer(m_commandBuffers[currentImage], &bufferBeginInfo));

  // Information about how to begin a render pass (only needed for graphical applications)
  VkRenderPassBeginInfo depthOnlyRenderPassBeginInfo = {};
  depthOnlyRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  depthOnlyRenderPassBeginInfo.renderPass = m_depthRenderPass;           // Render Pass to begin
  depthOnlyRenderPassBeginInfo.renderArea.offset = {0, 0};               // Start point of render pass in pixels
  depthOnlyRenderPassBeginInfo.renderArea.extent = {m_width, m_height};  // Size of region to run render pass on (starting at offset)

  std::array<VkClearValue, 1> depthOnlyClearValue = {};
  depthOnlyClearValue[0].depthStencil = {1.0f, 0};

  depthOnlyRenderPassBeginInfo.pClearValues = depthOnlyClearValue.data();  // List of clear values
  depthOnlyRenderPassBeginInfo.clearValueCount = static_cast<uint32_t>(depthOnlyClearValue.size());
  depthOnlyRenderPassBeginInfo.framebuffer = m_depthOnlyFramebuffer;

  if (g_RenderSetting.isOcclusionCulling) {
    // Must be done outside of render pass
    vkCmdResetQueryPool(m_commandBuffers[currentImage], m_occlusionQueryPool, 0,
                        static_cast<uint32_t>(g_BatchManager.m_meshes.size()));

    // Begin Render Pass
    vkCmdBeginRenderPass(m_commandBuffers[currentImage], &depthOnlyRenderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);  // 렌더 패스의 내용을 직접 명령 버퍼에 기록하는 것을 의미

    RecordOcclusionCullingCommands(currentImage);

    vkCmdEndRenderPass(m_commandBuffers[currentImage]);
  }
  VK_CHECK(vkEndCommandBuffer(m_commandBuffers[currentImage]));
}

void CullingRenderPass::RecordOcclusionCullingCommands(uint32_t currentImage) {
  /*
   * BoundingBox Renderer
   */
  std::vector<VkDrawIndexedIndirectCommand> commands{};
  for (auto& batch : g_BatchManager.m_miniBatchList) {
    commands.insert(commands.end(), batch.m_drawIndexedCommands.begin(), batch.m_drawIndexedCommands.end());
  }

  std::array<FrustumPlane, 6>& frustumPlanes = m_frustumPlanes;
  std::vector<std::future<void>> futures;
  futures.reserve(commands.size());

  for (int i = 0; i < commands.size(); ++i) {
    if (g_RenderSetting.isMultiThreading) {
      futures.push_back(g_ThreadPool.Submit([i, &commands, &frustumPlanes]() -> void {
        AABB aabb = g_BatchManager.m_boundingBoxList[i];
        glm::mat4& transform = g_BatchManager.m_trasformList[i].currentTransform;
        aabb.max = transform * aabb.max;
        aabb.min = transform * aabb.min;

        commands[i].instanceCount = (int)isAABBInsideFrustum(frustumPlanes, aabb);
      }));
    } else {
      AABB aabb = g_BatchManager.m_boundingBoxList[i];
      glm::mat4& transform = g_BatchManager.m_trasformList[i].currentTransform;
      aabb.max = transform * aabb.max;
      aabb.min = transform * aabb.min;

      commands[i].instanceCount = (int)isAABBInsideFrustum(m_frustumPlanes, aabb);
    }
  }
  if (g_RenderSetting.isMultiThreading) {
    for (auto& future : futures) {
      future.get();
    }
  }

  g_ShaderSetting.batchIdx = 0;
  {
    vkCmdBindPipeline(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthGraphicePipeline);

    for (int i = 0; i < commands.size(); ++i) {
      {
        VkDeviceSize vertexOffset = 0;  // Always bind at offset 0 since indirect commands handle offsets
        vkCmdBindVertexBuffers(m_commandBuffers[currentImage], 0, 1, &g_BatchManager.m_bbVertexBuffers[i].buffer, &vertexOffset);

        // Bind the index buffer with the correct offset
        vkCmdBindIndexBuffer(m_commandBuffers[currentImage], g_BatchManager.m_bbIndexBuffers[i].buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1,
                                &g_DescriptorManager.GetVkDescriptorSet("ViewProjection_ALL" + std::to_string(currentImage)), 0,
                                nullptr);
        vkCmdBindDescriptorSets(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 1, 1,
                                &g_DescriptorManager.GetVkDescriptorSet("BATCH_ALL" + std::to_string(currentImage)), 0, nullptr);
        vkCmdPushConstants(m_commandBuffers[currentImage], m_graphicsPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(ShaderSetting),
                           &g_ShaderSetting);

        vkCmdBeginQuery(m_commandBuffers[currentImage], m_occlusionQueryPool, g_ShaderSetting.batchIdx, 0);
        vkCmdDrawIndexed(m_commandBuffers[currentImage], g_BatchManager.m_meshes[i].indexCount, commands[i].instanceCount, 0, 0, 0);
        vkCmdEndQuery(m_commandBuffers[currentImage], m_occlusionQueryPool, g_ShaderSetting.batchIdx);
      }
      g_ShaderSetting.batchIdx += 1;
    }
  }
}
