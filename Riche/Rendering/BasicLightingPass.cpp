#include "BasicLightingPass.h"

BasicLightingPass::BasicLightingPass(VkDevice device, VkPhysicalDevice physicalDevice) : IRenderPass(device, physicalDevice) {}

void BasicLightingPass::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool,
                                   Camera* camera, Editor* editor, const uint32_t width, const uint32_t height) {
  m_pDevice = device;
  m_pPhyscialDevice = physicalDevice;
  m_pGraphicsQueue = queue;

  m_width = width;
  m_height = height;

  m_pCamera = camera;
  m_pEditor = editor;

  m_pGraphicsCommandPool = commandPool;

  CreateBuffers();
  CreateRenderPass();
  CreateFramebuffer();

  CreatePushConstantRange();
  CreateRaytracingDescriptorSets();

  CreatePipelineLayout();
  CreatePipeline();
  CreateShaderBindingTables();

  CreateSemaphores();
  CreateCommandBuffers();
}

void BasicLightingPass::Cleanup() {
  vkDestroyImageView(m_pDevice, m_colourBufferImageView, nullptr);
  vkDestroyImage(m_pDevice, m_colourBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_colourBufferImageMemory, nullptr);

  vkDestroyImageView(m_pDevice, m_depthStencilBufferImageView, nullptr);
  vkDestroyImage(m_pDevice, m_depthStencilBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_depthStencilBufferImageMemory, nullptr);

  vkDestroyImageView(m_pDevice, m_objectIdBufferImageView, nullptr);
  vkDestroyImage(m_pDevice, m_objectIdColourBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_objectIdBufferImageMemory, nullptr);

  vkDestroyImageView(m_pDevice, m_objectIdDepthStencilBufferImageView, nullptr);
  vkDestroyImage(m_pDevice, m_objectIdDepthStencilBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_objectIdDepthStencilBufferImageMemory, nullptr);

  vkDestroyFramebuffer(m_pDevice, m_framebuffer, nullptr);
  vkDestroyFramebuffer(m_pDevice, m_objectIdFramebuffer, nullptr);

  vkDestroySemaphore(m_pDevice, m_renderAvailable, nullptr);
  vkDestroyFence(m_pDevice, m_fence, nullptr);
  vkFreeCommandBuffers(m_pDevice, m_pGraphicsCommandPool, 1, &m_commandBuffer);

  vkDestroyPipeline(m_pDevice, m_graphicsPipeline, nullptr);
  vkDestroyPipeline(m_pDevice, m_wireGraphicsPipeline, nullptr);
  vkDestroyPipeline(m_pDevice, m_boundingBoxPipeline, nullptr);
  vkDestroyPipeline(m_pDevice, m_objectIDPipeline, nullptr);
  vkDestroyPipelineLayout(m_pDevice, m_graphicsPipelineLayout, nullptr);

  vkDestroyRenderPass(m_pDevice, m_renderPass, nullptr);
  vkDestroyRenderPass(m_pDevice, m_objectIdRenderPass, nullptr);

  vkDestroyBuffer(m_pDevice, m_bottomLevelAS.buffer, nullptr);
  vkDestroyAccelerationStructureKHR(m_pDevice, m_bottomLevelAS.handle, nullptr);
  vkFreeMemory(m_pDevice, m_bottomLevelAS.memory, nullptr);

  vkDestroyBuffer(m_pDevice, m_topLevelAS.buffer, nullptr);
  vkDestroyAccelerationStructureKHR(m_pDevice, m_topLevelAS.handle, nullptr);
  vkFreeMemory(m_pDevice, m_topLevelAS.memory, nullptr);

  vkDestroyPipeline(m_pDevice, m_raytracingPipeline, nullptr);
  vkDestroyPipelineLayout(m_pDevice, m_raytracingPipelineLayout, nullptr);

  vkFreeDescriptorSets(m_pDevice, m_raytracingPool, 1, &m_raytracingSet);
  vkDestroyDescriptorSetLayout(m_pDevice, m_raytracingSetLayout, nullptr);
  vkDestroyDescriptorPool(m_pDevice, m_raytracingPool, nullptr);
}

void BasicLightingPass::Update() {
  if (m_pCamera->isMousePressed) {
    m_pCamera->result = g_ResourceManager.ReadPixelFromImage(m_objectIdColourBufferImage, m_width, m_height, m_pCamera->MousePos().x,
                                                             m_pCamera->MousePos().y);

    std::cout << m_pCamera->MousePos().x << ", " << m_pCamera->MousePos().y << std::endl;
    std::cout << m_pCamera->result.r << std::endl;
  }
}

void BasicLightingPass::Draw(VkSemaphore renderAvailable) {
  vkWaitForFences(m_pDevice, 1, &m_fence, VK_TRUE, (std::numeric_limits<uint32_t>::max)());
  vkResetFences(m_pDevice, 1, &m_fence);

  RecordCommands();

  // RenderPass 1.
  VkSubmitInfo basicSubmitInfo = {};
  basicSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  basicSubmitInfo.waitSemaphoreCount = 1;  // Number of semaphores to wait on
  basicSubmitInfo.pWaitSemaphores = &renderAvailable;

  VkPipelineStageFlags basicWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
  basicSubmitInfo.pWaitDstStageMask = basicWaitStages;     // Stages to check semaphores at
  basicSubmitInfo.commandBufferCount = 1;                  // Number of command buffers to submit
  basicSubmitInfo.pCommandBuffers = &m_commandBuffer;      // Command buffer to submit
  basicSubmitInfo.signalSemaphoreCount = 1;                // Number of semaphores to signal
  basicSubmitInfo.pSignalSemaphores = &m_renderAvailable;  // Semaphores to signal when command buffer finishes
  // Command buffer가 실행을 완료하면, Signaled 상태가 될 semaphore 배열.

  VK_CHECK(vkQueueSubmit(m_pGraphicsQueue, 1, &basicSubmitInfo, m_fence));
}

void BasicLightingPass::CreateRenderPass() {
  // Array of Subpasses
  std::array<VkSubpassDescription, 1> subpasses{};

  // ATTACHMENTS
  // SUBPASS 1 ATTACHMENTS (INPUT ATTACHMEMNTS)
  // Colour Attachment
  VkAttachmentDescription colourAttachment = {};
  colourAttachment.format = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_R8G8B8A8_UNORM}, VK_IMAGE_TILING_OPTIMAL,
                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthStencilAttachment = {};
  depthStencilAttachment.format = VkUtils::ChooseSupportedFormat(
      m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colourAttachmentRef = {};
  colourAttachmentRef.attachment = 0;
  colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentRef = {};
  depthStencilAttachmentRef.attachment = 1;
  depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // Set up Subpass 1
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpasses[0].colorAttachmentCount = 1;
  subpasses[0].pColorAttachments = &colourAttachmentRef;
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
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dependencyFlags = 0;

  // Subpass 1 layout (colour/depth) to Externel(image/image)
  subpassDependencies[1].srcSubpass = 0;
  subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;  // We do not link swapchain. So, dstStageMask is to be SHADER_BIT.
  subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpassDependencies[1].dependencyFlags = 0;

  std::array<VkAttachmentDescription, 2> renderPassAttachments = {colourAttachment, depthStencilAttachment};

  // Create info for Render Pass
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
  renderPassCreateInfo.pAttachments = renderPassAttachments.data();
  renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
  renderPassCreateInfo.pSubpasses = subpasses.data();
  renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
  renderPassCreateInfo.pDependencies = subpassDependencies.data();

  VK_CHECK(vkCreateRenderPass(m_pDevice, &renderPassCreateInfo, nullptr, &m_renderPass));

  CreateObjectIdRenderPass();
  CreateRaytracingRenderPass();
}

void BasicLightingPass::CreateObjectIdRenderPass() {
  // Array of Subpasses
  std::array<VkSubpassDescription, 1> subpasses{};

  // ATTACHMENTS
  // SUBPASS 1 ATTACHMENTS (INPUT ATTACHMEMNTS)
  // Colour Attachment
  VkAttachmentDescription colourAttachment = {};
  colourAttachment.format = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_R32G32B32A32_SFLOAT}, VK_IMAGE_TILING_OPTIMAL,
                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthStencilAttachment = {};
  depthStencilAttachment.format = VkUtils::ChooseSupportedFormat(
      m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colourAttachmentRef = {};
  colourAttachmentRef.attachment = 0;
  colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentRef = {};
  depthStencilAttachmentRef.attachment = 1;
  depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // Set up Subpass 1
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpasses[0].colorAttachmentCount = 1;
  subpasses[0].pColorAttachments = &colourAttachmentRef;
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
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dependencyFlags = 0;

  // Subpass 1 layout (colour/depth) to Externel(image/image)
  subpassDependencies[1].srcSubpass = 0;
  subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;  // We do not link swapchain. So, dstStageMask is to be SHADER_BIT.
  subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  subpassDependencies[1].dependencyFlags = 0;

  std::array<VkAttachmentDescription, 2> renderPassAttachments = {colourAttachment, depthStencilAttachment};

  // Create info for Render Pass
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
  renderPassCreateInfo.pAttachments = renderPassAttachments.data();
  renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
  renderPassCreateInfo.pSubpasses = subpasses.data();
  renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
  renderPassCreateInfo.pDependencies = subpassDependencies.data();

  VK_CHECK(vkCreateRenderPass(m_pDevice, &renderPassCreateInfo, nullptr, &m_objectIdRenderPass));
}

void BasicLightingPass::CreateRaytracingRenderPass() {}

void BasicLightingPass::CreateFramebuffer() {
  VkFormat colourImageFormat = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_R8G8B8A8_UNORM}, VK_IMAGE_TILING_OPTIMAL,
                                                              VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_colourBufferImageMemory, &m_colourBufferImage,
                         colourImageFormat,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkUtils::CreateImageView(m_pDevice, m_colourBufferImage, &m_colourBufferImageView, colourImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

  VkFormat depthImageFormat = VkUtils::ChooseSupportedFormat(
      m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_depthStencilBufferImageMemory, &m_depthStencilBufferImage,
                         depthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkUtils::CreateImageView(m_pDevice, m_depthStencilBufferImage, &m_depthStencilBufferImageView, depthImageFormat,
                           VK_IMAGE_ASPECT_DEPTH_BIT);

  std::array<VkImageView, 2> attachments = {m_colourBufferImageView, m_depthStencilBufferImageView};

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.renderPass = m_renderPass;  // Render pass layout the framebuffer will be used with
  framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferCreateInfo.pAttachments = attachments.data();  // List of attachments (1:1 with render pass)
  framebufferCreateInfo.width = m_width;                    // framebuffer width
  framebufferCreateInfo.height = m_height;                  // framebuffer height
  framebufferCreateInfo.layers = 1;                         // framebuffer layers

  VK_CHECK(vkCreateFramebuffer(m_pDevice, &framebufferCreateInfo, nullptr, &m_framebuffer));

  CreateObjectIdFramebuffer();
  CreateRaytracingFramebuffer();
}

void BasicLightingPass::CreateObjectIdFramebuffer() {
  VkFormat colourImageFormat = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_R32G32B32A32_SFLOAT},
                                                              VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_objectIdBufferImageMemory, &m_objectIdColourBufferImage,
                         colourImageFormat,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkUtils::CreateImageView(m_pDevice, m_objectIdColourBufferImage, &m_objectIdBufferImageView, colourImageFormat,
                           VK_IMAGE_ASPECT_COLOR_BIT);

  VkFormat depthImageFormat = VkUtils::ChooseSupportedFormat(
      m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(
      m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_objectIdDepthStencilBufferImageMemory, &m_objectIdDepthStencilBufferImage,
      depthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkUtils::CreateImageView(m_pDevice, m_objectIdDepthStencilBufferImage, &m_objectIdDepthStencilBufferImageView, depthImageFormat,
                           VK_IMAGE_ASPECT_DEPTH_BIT);

  std::array<VkImageView, 2> attachments = {m_objectIdBufferImageView, m_objectIdDepthStencilBufferImageView};

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.renderPass = m_objectIdRenderPass;  // Render pass layout the framebuffer will be used with
  framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferCreateInfo.pAttachments = attachments.data();  // List of attachments (1:1 with render pass)
  framebufferCreateInfo.width = m_width;                    // framebuffer width
  framebufferCreateInfo.height = m_height;                  // framebuffer height
  framebufferCreateInfo.layers = 1;                         // framebuffer layers

  VK_CHECK(vkCreateFramebuffer(m_pDevice, &framebufferCreateInfo, nullptr, &m_objectIdFramebuffer));
}

void BasicLightingPass::CreateRaytracingFramebuffer() {
  VkFormat colourImageFormat = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_R8G8B8A8_UNORM}, VK_IMAGE_TILING_OPTIMAL,
                                                              VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_raytracingImageMemory, &m_raytracingImage,
                         colourImageFormat,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkUtils::CreateImageView(m_pDevice, m_raytracingImage, &m_raytracingImageView, colourImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

  VkUtils::DescriptorBuilder raytracingBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  VkDescriptorImageInfo shadowImageInfo{VK_NULL_HANDLE, m_raytracingImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  raytracingBuilder.BindImage(0, &shadowImageInfo, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL);
  g_DescriptorManager.AddDescriptorSet(&raytracingBuilder, "ShadowTexture_ALL");
}

void BasicLightingPass::CreatePipelineLayout() { CreateRaytracingPipelineLayout(); }

void BasicLightingPass::CreateRaytracingPipelineLayout() {
  // -- PIPELINE LAYOUT (It's like Root signature in D3D12) --
  std::vector<VkDescriptorSetLayout> setLayouts = {
      g_DescriptorManager.GetVkDescriptorSetLayout("ViewProjection_ALL"),
      m_raytracingSetLayout,
  };

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = setLayouts.size();
  pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &m_debugPushConstant;

  VK_CHECK(vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &m_raytracingPipelineLayout));
}

void BasicLightingPass::CreatePipeline() {
  CraeteGraphicsPipeline();
  CreateWireGraphicsPipeline();
  CreateBoundingBoxPipeline();
  CreateObjectIDPipeline();
  CreateRaytracingPipeline();
}

void BasicLightingPass::CraeteGraphicsPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/LightingVS.spv");
  auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/LightingPS.spv");

  // Build Shaders
  VkShaderModule vertexShaderModule = VkUtils::CreateShaderModule(m_pDevice, vertexShaderCode);
  VkShaderModule fragmentShaderModule = VkUtils::CreateShaderModule(m_pDevice, fragmentShaderCode);

  // Set new shaders
  // Vertex Stage Creation information
  VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
  vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // Shader stage name
  vertexShaderCreateInfo.module = vertexShaderModule;         // Shader moudle to be used by stage
  vertexShaderCreateInfo.pName = "main";                      // Entry point in to shader
  // Fragment Stage Creation information
  VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
  fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // Shader stage name
  fragmentShaderCreateInfo.module = fragmentShaderModule;         // Shader moudle to be used by stage
  fragmentShaderCreateInfo.pName = "main";                        // Entry point in to shader

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

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
  colourState.blendEnable = VK_TRUE;                      // Enable Blending

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
  colourBlendingCreateInfo.attachmentCount = 1;
  colourBlendingCreateInfo.pAttachments = &colourState;

  // -- PIPELINE LAYOUT (It's like Root signature in D3D12) --
  std::vector<VkDescriptorSetLayout> setLayouts = {g_DescriptorManager.GetVkDescriptorSetLayout("ViewProjection_ALL"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("Transform_ALL"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("SamplerList_ALL"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("ObjectID"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("DiffuseTextureList"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("ShadowTexture_ALL")};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = setLayouts.size();
  pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &m_debugPushConstant;

  VK_CHECK(vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &m_graphicsPipelineLayout));

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
  pipelineCreateInfo.stageCount = 2;                              // Number of shader stages
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
  pipelineCreateInfo.renderPass = m_renderPass;          // Render pass description the pipeline is compatible with
  pipelineCreateInfo.subpass = 0;                        // Subpass of render pass to use with pipeline

  // Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;  // Existing pipline to derive from
  pipelineCreateInfo.basePipelineIndex = -1;  // or index of pipeline being created to derive from (in case createing multiple at once)

  VK_CHECK(vkCreateGraphicsPipelines(m_pDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphicsPipeline));

  // Destroy second shader modules
  vkDestroyShaderModule(m_pDevice, vertexShaderModule, nullptr);
  vkDestroyShaderModule(m_pDevice, fragmentShaderModule, nullptr);
}

void BasicLightingPass::CreateWireGraphicsPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/LightingVS.spv");
  auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/LightingPS.spv");

  // Build Shaders
  VkShaderModule vertexShaderModule = VkUtils::CreateShaderModule(m_pDevice, vertexShaderCode);
  VkShaderModule fragmentShaderModule = VkUtils::CreateShaderModule(m_pDevice, fragmentShaderCode);

  // Set new shaders
  // Vertex Stage Creation information
  VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
  vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // Shader stage name
  vertexShaderCreateInfo.module = vertexShaderModule;         // Shader moudle to be used by stage
  vertexShaderCreateInfo.pName = "main";                      // Entry point in to shader
  // Fragment Stage Creation information
  VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
  fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // Shader stage name
  fragmentShaderCreateInfo.module = fragmentShaderModule;         // Shader moudle to be used by stage
  fragmentShaderCreateInfo.pName = "main";                        // Entry point in to shader

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

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
  rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;  // How to handle filling points between vertices.
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
  colourState.blendEnable = VK_TRUE;                      // Enable Blending

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
  colourBlendingCreateInfo.attachmentCount = 1;
  colourBlendingCreateInfo.pAttachments = &colourState;

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
  pipelineCreateInfo.stageCount = 2;                              // Number of shader stages
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
  pipelineCreateInfo.renderPass = m_renderPass;          // Render pass description the pipeline is compatible with
  pipelineCreateInfo.subpass = 0;                        // Subpass of render pass to use with pipeline

  // Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;  // Existing pipline to derive from
  pipelineCreateInfo.basePipelineIndex = -1;  // or index of pipeline being created to derive from (in case createing multiple at once)

  VK_CHECK(vkCreateGraphicsPipelines(m_pDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_wireGraphicsPipeline));

  // Destroy second shader modules
  vkDestroyShaderModule(m_pDevice, vertexShaderModule, nullptr);
  vkDestroyShaderModule(m_pDevice, fragmentShaderModule, nullptr);
}

void BasicLightingPass::CreateBoundingBoxPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/BoundingBoxVS.spv");
  auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/BoundingBoxPS.spv");

  // Build Shaders
  VkShaderModule vertexShaderModule = VkUtils::CreateShaderModule(m_pDevice, vertexShaderCode);
  VkShaderModule fragmentShaderModule = VkUtils::CreateShaderModule(m_pDevice, fragmentShaderCode);

  // Set new shaders
  // Vertex Stage Creation information
  VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
  vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // Shader stage name
  vertexShaderCreateInfo.module = vertexShaderModule;         // Shader moudle to be used by stage
  vertexShaderCreateInfo.pName = "main";                      // Entry point in to shader
  // Fragment Stage Creation information
  VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
  fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // Shader stage name
  fragmentShaderCreateInfo.module = fragmentShaderModule;         // Shader moudle to be used by stage
  fragmentShaderCreateInfo.pName = "main";                        // Entry point in to shader

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

  // -- VERTEX INPUT --
  // -- Create Pipeline --
  // How the data for a single vertex (including info suach as position, colour, textuer coords, normals, etc) is as a whole.
  VkVertexInputBindingDescription bindingDescription = {};
  bindingDescription.binding = 0;                                        // Can bind multiple streams of data, this defines which one
  bindingDescription.stride = static_cast<uint32_t>(sizeof(glm::vec3));  // size of a single vertex object
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;            // How to move between data after each vertex
  // VK_VERTEX_INPUT_RATE_VERTEX		: Move on to the next vertex
  // VK_VERTEX_INPUT_RATE_INSTANCE	: Move to a vertex for the next instance

  // How the data for an attribute is defined whithin a vertex
  std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions;
  // Position Attribute
  attributeDescriptions[0].binding = 0;                          // Which binding the data is at (should be same as above)
  attributeDescriptions[0].location = 0;                         // Location in shader where data will be read from
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // Format the data will take (also helps define size of data)
  attributeDescriptions[0].offset = 0;                           // Where this attribute is defined in the data for a single vertex.

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
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;  // Primitive type to assemble vertices
  inputAssembly.primitiveRestartEnable = VK_FALSE;           // Allow overriding of "strip" topology to start new primitives

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
  rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;  // How to handle filling points between vertices.
  rasterizerCreateInfo.lineWidth = 2.0f;                    // How thick lines should be when drawn
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
  colourState.blendEnable = VK_TRUE;                      // Enable Blending

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
  colourBlendingCreateInfo.attachmentCount = 1;
  colourBlendingCreateInfo.pAttachments = &colourState;

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
  pipelineCreateInfo.stageCount = 2;                              // Number of shader stages
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
  pipelineCreateInfo.renderPass = m_renderPass;          // Render pass description the pipeline is compatible with
  pipelineCreateInfo.subpass = 0;                        // Subpass of render pass to use with pipeline

  // Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;  // Existing pipline to derive from
  pipelineCreateInfo.basePipelineIndex = -1;  // or index of pipeline being created to derive from (in case createing multiple at once)

  VK_CHECK(vkCreateGraphicsPipelines(m_pDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_boundingBoxPipeline));

  // Destroy second shader modules
  vkDestroyShaderModule(m_pDevice, vertexShaderModule, nullptr);
  vkDestroyShaderModule(m_pDevice, fragmentShaderModule, nullptr);
}

void BasicLightingPass::CreateObjectIDPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/ObjectIdVS.spv");
  auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/ObjectIdPS.spv");

  // Build Shaders
  VkShaderModule vertexShaderModule = VkUtils::CreateShaderModule(m_pDevice, vertexShaderCode);
  VkShaderModule fragmentShaderModule = VkUtils::CreateShaderModule(m_pDevice, fragmentShaderCode);

  // Set new shaders
  // Vertex Stage Creation information
  VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
  vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // Shader stage name
  vertexShaderCreateInfo.module = vertexShaderModule;         // Shader moudle to be used by stage
  vertexShaderCreateInfo.pName = "main";                      // Entry point in to shader
  // Fragment Stage Creation information
  VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
  fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // Shader stage name
  fragmentShaderCreateInfo.module = fragmentShaderModule;         // Shader moudle to be used by stage
  fragmentShaderCreateInfo.pName = "main";                        // Entry point in to shader

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

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
  colourState.blendEnable = VK_TRUE;                      // Enable Blending

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
  colourBlendingCreateInfo.attachmentCount = 1;
  colourBlendingCreateInfo.pAttachments = &colourState;

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
  pipelineCreateInfo.stageCount = 2;                              // Number of shader stages
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
  pipelineCreateInfo.renderPass = m_objectIdRenderPass;  // Render pass description the pipeline is compatible with
  pipelineCreateInfo.subpass = 0;                        // Subpass of render pass to use with pipeline

  // Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;  // Existing pipline to derive from
  pipelineCreateInfo.basePipelineIndex = -1;  // or index of pipeline being created to derive from (in case createing multiple at once)

  VK_CHECK(vkCreateGraphicsPipelines(m_pDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_objectIDPipeline));

  // Destroy second shader modules
  vkDestroyShaderModule(m_pDevice, vertexShaderModule, nullptr);
  vkDestroyShaderModule(m_pDevice, fragmentShaderModule, nullptr);
}

void BasicLightingPass::CreateRaytracingPipeline() {
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

  // Ray generation group
  {
    auto shaderCode = VkUtils::ReadFile("Resources/Shaders/RaytracingShadow/Raygen.rgen.spv");
    VkShaderModule shaderModule = VkUtils::CreateShaderModule(m_pDevice, shaderCode);
    VkPipelineShaderStageCreateInfo stageCreateInfo = {};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;  // Shader stage name
    stageCreateInfo.module = shaderModule;                   // Shader moudle to be used by stage
    stageCreateInfo.pName = "main";
    shaderStages.push_back(stageCreateInfo);

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
    shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
    shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(shaderGroup);
  }

  // Miss group
  {
    auto shaderCode = VkUtils::ReadFile("Resources/Shaders/RaytracingShadow/Miss.rmiss.spv");
    VkShaderModule shaderModule = VkUtils::CreateShaderModule(m_pDevice, shaderCode);
    VkPipelineShaderStageCreateInfo stageCreateInfo = {};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;  // Shader stage name
    stageCreateInfo.module = shaderModule;                 // Shader moudle to be used by stage
    stageCreateInfo.pName = "main";
    shaderStages.push_back(stageCreateInfo);

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
    shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
    shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(shaderGroup);

    // Second shader for shadows
    auto secondShaderCode = VkUtils::ReadFile("Resources/Shaders/RaytracingShadow/Shadow.rmiss.spv");
    VkShaderModule secondShaderModule = VkUtils::CreateShaderModule(m_pDevice, secondShaderCode);
    VkPipelineShaderStageCreateInfo secondStageCreateInfo = {};
    secondStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    secondStageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;  // Shader stage name
    secondStageCreateInfo.module = secondShaderModule;           // Shader moudle to be used by stage
    secondStageCreateInfo.pName = "main";
    shaderStages.push_back(secondStageCreateInfo);
    shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
    shaderGroups.push_back(shaderGroup);
  }

  // Closest hit group
  {
    auto shaderCode = VkUtils::ReadFile("Resources/Shaders/RaytracingShadow/ClosestHit.rchit.spv");
    VkShaderModule shaderModule = VkUtils::CreateShaderModule(m_pDevice, shaderCode);
    VkPipelineShaderStageCreateInfo stageCreateInfo = {};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;  // Shader stage name
    stageCreateInfo.module = shaderModule;                        // Shader moudle to be used by stage
    stageCreateInfo.pName = "main";
    shaderStages.push_back(stageCreateInfo);

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
    shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
    shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(shaderGroup);
  }

  VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo{};
  rayTracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
  rayTracingPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  rayTracingPipelineCreateInfo.pStages = shaderStages.data();
  rayTracingPipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
  rayTracingPipelineCreateInfo.pGroups = shaderGroups.data();
  rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = std::min(uint32_t(2), rayTracingPipelineProperties.maxRayRecursionDepth);
  rayTracingPipelineCreateInfo.layout = m_raytracingPipelineLayout;

  VK_CHECK(vkCreateRayTracingPipelinesKHR(m_pDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCreateInfo, nullptr,
                                          &m_raytracingPipeline));

  for (const auto& stage : shaderStages) {
    vkDestroyShaderModule(m_pDevice, stage.module, nullptr);
  }
}

void BasicLightingPass::CreateBuffers() {
  CreateBindlessResources();
  CreateRaytracingBuffers();
}

void BasicLightingPass::CreateBindlessResources() {
  void* pData = nullptr;

  // ObjectID
  VkDeviceSize idBufferSize = static_cast<uint64_t>(g_BatchManager.m_meshIDList.size() * sizeof(ObjectID));
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, idBufferSize,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &g_BatchManager.m_objectIDListBuffer, &g_BatchManager.m_objectIDListBufferMemory);

  vkMapMemory(m_pDevice, g_BatchManager.m_objectIDListBufferMemory, 0, idBufferSize, 0, &pData);
  memcpy(pData, g_BatchManager.m_meshIDList.data(), idBufferSize);
  vkUnmapMemory(m_pDevice, g_BatchManager.m_objectIDListBufferMemory);

  VkDescriptorBufferInfo idUBOInfo = {};
  idUBOInfo.buffer = g_BatchManager.m_objectIDListBuffer;  // Buffer to get data from
  idUBOInfo.offset = 0;                                    // Position of start of data
  idUBOInfo.range = idBufferSize;                          // size of data

  VkUtils::DescriptorBuilder idBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  idBuilder.BindBuffer(0, &idUBOInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, true);
  g_DescriptorManager.AddDescriptorSet(&idBuilder, "ObjectID", true);

  VkDeviceSize imageListSize = 0;
  for (VkDeviceSize& size : g_BatchManager.m_diffuseImageListSize) {
    imageListSize += size;
  }

  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.resize(g_BatchManager.m_diffuseImageViewList.size());
  for (size_t i = 0; i < g_BatchManager.m_diffuseImageViewList.size(); ++i) {
    imageInfos[i].imageView = g_BatchManager.m_diffuseImageViewList[i];
    imageInfos[i].sampler = VK_NULL_HANDLE;
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  VkUtils::DescriptorBuilder materialBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  materialBuilder.BindImage(0, imageInfos.data(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, true, imageInfos.size());
  g_DescriptorManager.AddDescriptorSet(&materialBuilder, "DiffuseTextureList", true);
}

void BasicLightingPass::CreateRaytracingBuffers() {
  void* pData = nullptr;

  g_BatchManager.m_vertiesBufferSize = static_cast<uint64_t>(g_BatchManager.m_allMeshVertices.size() * sizeof(RayTracingVertex));
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, g_BatchManager.m_vertiesBufferSize,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &g_BatchManager.m_vertiesBuffer,
                        &g_BatchManager.m_vertiesBufferMemory, true);

  vkMapMemory(m_pDevice, g_BatchManager.m_vertiesBufferMemory, 0, g_BatchManager.m_vertiesBufferSize, 0, &pData);
  memcpy(pData, g_BatchManager.m_allMeshVertices.data(), g_BatchManager.m_vertiesBufferSize);
  vkUnmapMemory(m_pDevice, g_BatchManager.m_vertiesBufferMemory);

  g_BatchManager.m_indicesBufferSize = static_cast<uint64_t>(g_BatchManager.m_allMeshIndices.size() * sizeof(uint32_t));
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, g_BatchManager.m_indicesBufferSize,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &g_BatchManager.m_indicesBuffer,
                        &g_BatchManager.m_indicesBufferMemory, true);

  vkMapMemory(m_pDevice, g_BatchManager.m_indicesBufferMemory, 0, g_BatchManager.m_indicesBufferSize, 0, &pData);
  memcpy(pData, g_BatchManager.m_allMeshIndices.data(), g_BatchManager.m_indicesBufferSize);
  vkUnmapMemory(m_pDevice, g_BatchManager.m_indicesBufferMemory);

  CreateBLAS();
  CreateTLAS();
}

void BasicLightingPass::CreateRaytracingDescriptorSets() {
  std::vector<VkDescriptorPoolSize> poolSizes = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                                 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                                                 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}};
  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolCreateInfo.maxSets = 1;
  descriptorPoolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
  descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
  VK_CHECK(vkCreateDescriptorPool(m_pDevice, &descriptorPoolCreateInfo, nullptr, &m_raytracingPool));

  std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
      VkUtils::DescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_ALL),
      VkUtils::DescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL),
      VkUtils::DescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL),
      VkUtils::DescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)};

  VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
  setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
  setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
  vkCreateDescriptorSetLayout(m_pDevice, &setLayoutCreateInfo, nullptr, &m_raytracingSetLayout);

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
  descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.descriptorPool = m_raytracingPool;
  descriptorSetAllocateInfo.descriptorSetCount = 1;
  descriptorSetAllocateInfo.pSetLayouts = &m_raytracingSetLayout;
  VK_CHECK(vkAllocateDescriptorSets(m_pDevice, &descriptorSetAllocateInfo, &m_raytracingSet));

  VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
  descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
  descriptorAccelerationStructureInfo.pAccelerationStructures = &m_topLevelAS.handle;

  VkWriteDescriptorSet accelerationStructureWrite{};
  accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  // The specialized acceleration structure descriptor has to be chained
  accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
  accelerationStructureWrite.dstSet = m_raytracingSet;
  accelerationStructureWrite.dstBinding = 0;
  accelerationStructureWrite.descriptorCount = 1;
  accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

  VkDescriptorImageInfo storageImageDescriptor{VK_NULL_HANDLE, m_raytracingImageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorBufferInfo vertexBufferDescriptor{g_BatchManager.m_vertiesBuffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo indexBufferDescriptor{g_BatchManager.m_indicesBuffer, 0, VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      // Binding 0: Top level acceleration structure
      accelerationStructureWrite,
      // Binding 1: Ray tracing result image
      VkUtils::WriteDescriptorSet(m_raytracingSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
      // Binding 3: Scene vertex buffer
      VkUtils::WriteDescriptorSet(m_raytracingSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &vertexBufferDescriptor),
      // Binding 4: Scene index buffer
      VkUtils::WriteDescriptorSet(m_raytracingSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &indexBufferDescriptor),
  };
  vkUpdateDescriptorSets(m_pDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

/*
    Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void BasicLightingPass::CreateBLAS() {
  VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
  VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

  vertexBufferDeviceAddress.deviceAddress =
      GetVkDeviceAddress(m_pDevice, g_BatchManager.m_vertiesBuffer);
  indexBufferDeviceAddress.deviceAddress = GetVkDeviceAddress(m_pDevice, g_BatchManager.m_indicesBuffer);

  uint32_t numTriangles = static_cast<uint32_t>(g_BatchManager.m_allMeshIndices.size()) / 3;

  // Build
  VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
  accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
  accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
  accelerationStructureGeometry.geometry.triangles.maxVertex = g_BatchManager.m_allMeshVertices.size() - 1;
  accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(RayTracingVertex);
  accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
  accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
  accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
  accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;

  // Get size info
  VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
  accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  accelerationStructureBuildGeometryInfo.flags =
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
  accelerationStructureBuildGeometryInfo.geometryCount = 1;
  accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

  VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
  accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

  vkGetAccelerationStructureBuildSizesKHR(m_pDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                          &accelerationStructureBuildGeometryInfo, &numTriangles,
                                          &accelerationStructureBuildSizesInfo);

  CreateAccelerationStructure(m_pDevice, m_pPhyscialDevice, m_bottomLevelAS, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                              accelerationStructureBuildSizesInfo);

  ScratchBuffer scratchBuffer =
      CreateScratchBuffer(m_pDevice, m_pPhyscialDevice, accelerationStructureBuildSizesInfo.buildScratchSize);

  VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
  accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  accelerationBuildGeometryInfo.dstAccelerationStructure = m_bottomLevelAS.handle;
  accelerationBuildGeometryInfo.geometryCount = 1;
  accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
  accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

  VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
  accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
  accelerationStructureBuildRangeInfo.primitiveOffset = 0;
  accelerationStructureBuildRangeInfo.firstVertex = 0;
  accelerationStructureBuildRangeInfo.transformOffset = 0;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = {&accelerationStructureBuildRangeInfo};

  // Build the acceleration structure on the device via a one-time command buffer submission
  // Some implementations may support acceleration structure building on the host
  // (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
  VkCommandBuffer commandBuffer = g_ResourceManager.CreateAndBeginCommandBuffer();
  vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());
  g_ResourceManager.EndAndSummitCommandBuffer(commandBuffer);

  if (scratchBuffer.memory != VK_NULL_HANDLE) {
    vkFreeMemory(m_pDevice, scratchBuffer.memory, nullptr);
  }
  if (scratchBuffer.handle != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_pDevice, scratchBuffer.handle, nullptr);
  }
}

/*
    The top level acceleration structure contains the scene's object instances
*/
void BasicLightingPass::CreateTLAS() {
  // uint32_t numInstances = (uint32_t)g_BatchManager.m_trasformList.size();
  // std::vector<VkAccelerationStructureInstanceKHR> instances(numInstances);

  // for (uint32_t i = 0; i < numInstances; i++) {
  //   // 각 인스턴스별 변환행렬, customIndex, mask 등 세팅
  //   VkAccelerationStructureInstanceKHR& instance = instances[i];

  //  glm::mat4 curr = (g_BatchManager.m_trasformList[i].currentTransform);
  //  VkTransformMatrixKHR transformMatrix = mat4ToVkTransform(curr);
  //  instance.transform = transformMatrix;

  //  // 예: 하위 구조(= BLAS) 주소
  //  instance.accelerationStructureReference = m_bottomLevelAS.deviceAddress;

  //  // 그 외 속성
  //  instance.instanceCustomIndex = i;  // 임의의 식별자
  //  instance.mask = 0xFF;
  //  instance.instanceShaderBindingTableRecordOffset = 0;
  //  instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  //}

  VkTransformMatrixKHR transformMatrix = {0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f};

  VkAccelerationStructureInstanceKHR instance{};
  instance.transform = transformMatrix;
  instance.instanceCustomIndex = 0;
  instance.mask = 0xFF;
  instance.instanceShaderBindingTableRecordOffset = 0;
  instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  instance.accelerationStructureReference = m_bottomLevelAS.deviceAddress;

  VkBuffer instanceBuffer;
  VkDeviceMemory instanceBufferMemory;
  VkUtils::CreateBuffer(
      m_pDevice, m_pPhyscialDevice, sizeof(VkAccelerationStructureInstanceKHR),
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &instanceBuffer, &instanceBufferMemory, true);

  void* mappedData = nullptr;
  vkMapMemory(m_pDevice, instanceBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &mappedData);
  memcpy(mappedData, &instance, sizeof(VkAccelerationStructureInstanceKHR));
  vkUnmapMemory(m_pDevice, instanceBufferMemory);

  VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
  instanceDataDeviceAddress.deviceAddress = GetVkDeviceAddress(m_pDevice, instanceBuffer);

  VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
  accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
  accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

  VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
  accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  accelerationStructureBuildGeometryInfo.geometryCount = 1;
  accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

  uint32_t primitive_count = 1;

  // Get Size Info
  VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
  accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  vkGetAccelerationStructureBuildSizesKHR(m_pDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                          &accelerationStructureBuildGeometryInfo, &primitive_count,
                                          &accelerationStructureBuildSizesInfo);

  CreateAccelerationStructure(m_pDevice, m_pPhyscialDevice, m_topLevelAS, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                              accelerationStructureBuildSizesInfo);

  ScratchBuffer scratchBuffer =
      CreateScratchBuffer(m_pDevice, m_pPhyscialDevice, accelerationStructureBuildSizesInfo.buildScratchSize);

  VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
  accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  accelerationBuildGeometryInfo.dstAccelerationStructure = m_topLevelAS.handle;
  accelerationBuildGeometryInfo.geometryCount = 1;
  accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
  accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

  VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
  accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
  accelerationStructureBuildRangeInfo.primitiveOffset = 0;
  accelerationStructureBuildRangeInfo.firstVertex = 0;
  accelerationStructureBuildRangeInfo.transformOffset = 0;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = {&accelerationStructureBuildRangeInfo};

  // Build the acceleration structure on the device via a one-time command buffer submission
  // Some implementations may support acceleration structure building on the host
  // (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
  VkCommandBuffer commandBuffer = g_ResourceManager.CreateAndBeginCommandBuffer();
  vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());
  g_ResourceManager.EndAndSummitCommandBuffer(commandBuffer);

  if (scratchBuffer.memory != VK_NULL_HANDLE) {
    vkFreeMemory(m_pDevice, scratchBuffer.memory, nullptr);
  }
  if (scratchBuffer.handle != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_pDevice, scratchBuffer.handle, nullptr);
  }
  vkDestroyBuffer(m_pDevice, instanceBuffer, nullptr);
  vkFreeMemory(m_pDevice, instanceBufferMemory, nullptr);
}

void BasicLightingPass::CreateShaderBindingTables() {
  const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
  const uint32_t handleSizeAligned = VkUtils::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize,
                                                          rayTracingPipelineProperties.shaderGroupHandleAlignment);
  const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
  const uint32_t sbtSize = groupCount * handleSizeAligned;

  std::vector<uint8_t> shaderHandleStorage(sbtSize);
  VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_pDevice, m_raytracingPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

  CreateShaderBindingTable(m_pDevice, m_pPhyscialDevice, shaderBindingTables.raygen, 1);
  // We are using two miss shaders
  CreateShaderBindingTable(m_pDevice, m_pPhyscialDevice, shaderBindingTables.miss, 2);
  CreateShaderBindingTable(m_pDevice, m_pPhyscialDevice, shaderBindingTables.hit, 1);

  // Copy handles
  void* pData = nullptr;
  VK_CHECK(vkMapMemory(m_pDevice, shaderBindingTables.raygen.memory, 0, handleSize, 0, &pData));
  memcpy(pData, shaderHandleStorage.data(), handleSize);
  vkUnmapMemory(m_pDevice, shaderBindingTables.raygen.memory);
  VK_CHECK(vkMapMemory(m_pDevice, shaderBindingTables.miss.memory, 0, handleSize * 2, 0, &pData));
  memcpy(pData, shaderHandleStorage.data() + handleSizeAligned, handleSize * 2);
  vkUnmapMemory(m_pDevice, shaderBindingTables.miss.memory);
  VK_CHECK(vkMapMemory(m_pDevice, shaderBindingTables.hit.memory, 0, handleSize, 0, &pData));
  memcpy(pData, shaderHandleStorage.data() + handleSizeAligned * 3, handleSize);
  vkUnmapMemory(m_pDevice, shaderBindingTables.hit.memory);
}

void BasicLightingPass::CreatePushConstantRange() {
  m_debugPushConstant.stageFlags = VK_SHADER_STAGE_ALL;  // Shader stage push constant will go to
  m_debugPushConstant.offset = 0;                        // Offset into given data to pass to push constant
  m_debugPushConstant.size = sizeof(ShaderSetting);      // Size of Data Being Passed
}

void BasicLightingPass::CreateSemaphores() {
  // Semaphore creataion information
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(m_pDevice, &fenceCreateInfo, nullptr, &m_fence));
  VK_CHECK(vkCreateSemaphore(m_pDevice, &semaphoreCreateInfo, nullptr, &m_renderAvailable));
}

void BasicLightingPass::CreateCommandBuffers() {
  //
  // Single Command Buffer
  //
  VkCommandBufferAllocateInfo cbAllocInfo = {};
  cbAllocInfo = {};
  cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAllocInfo.commandPool = m_pGraphicsCommandPool;  // 해당 큐 패밀리의 큐에서만 커맨드 큐 동작이 실행가능하다.
  cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to
                                                        // queue. cant be called by other buffers
  // VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via
  // 'vkCmdExecuteCommands' when recording commands in primary buffer.
  cbAllocInfo.commandBufferCount = 1;

  // Allocate command buffers and place handles in array of buffers
  VK_CHECK(vkAllocateCommandBuffers(m_pDevice, &cbAllocInfo, &m_commandBuffer));
}

void BasicLightingPass::RecordCommands() {
  VkCommandBufferBeginInfo bufferBeginInfo = {};
  bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;  // Buffer can be resubmitted when it has already been
                                                                         // submitted and is awaiting executi

  // Start recording commands to command buffer!
  VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &bufferBeginInfo));

  VkUtils::CmdImageBarrier(m_commandBuffer, m_raytracingImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_raytracingPipeline);
  vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_raytracingPipelineLayout, 0, 1,
                          &g_DescriptorManager.GetVkDescriptorSet("ViewProjection_ALL"), 0, nullptr);
  vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_raytracingPipelineLayout, 1, 1, &m_raytracingSet,
                          0, nullptr);
  vkCmdPushConstants(m_commandBuffer, m_raytracingPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(ShaderSetting), &g_ShaderSetting);

  VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
  vkCmdTraceRaysKHR(m_commandBuffer, &shaderBindingTables.raygen.stridedDeviceAddressRegion,
                    &shaderBindingTables.miss.stridedDeviceAddressRegion, &shaderBindingTables.hit.stridedDeviceAddressRegion,
                    &emptySbtEntry, m_width, m_height, 1);

  VkUtils::CmdImageBarrier(m_commandBuffer, m_raytracingImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  // Information about how to begin a render pass (only needed for graphical applications)
  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.renderPass = m_renderPass;                // Render Pass to begin
  renderPassBeginInfo.renderArea.offset = {0, 0};               // Start point of render pass in pixels
  renderPassBeginInfo.renderArea.extent = {m_width, m_height};  // Size of region to run render pass on (starting at offset)

  std::array<VkClearValue, 2> clearValues = {};
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
  clearValues[1].depthStencil.depth = 1.0f;

  renderPassBeginInfo.pClearValues = clearValues.data();  // List of clear values
  renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

  renderPassBeginInfo.framebuffer = m_framebuffer;

  // Begin Render Pass
  vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Bind Pipeline to be used in render pass
  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    g_RenderSetting.isWireRendering ? m_wireGraphicsPipeline : m_graphicsPipeline);
  //
  // mini-batch system
  //
  g_ShaderSetting.batchIdx = 0;
  int batchIndex = 0;
  for (auto& miniBatch : g_BatchManager.m_miniBatchList) {
    // Bind the vertex buffer with the correct offset
    VkDeviceSize vertexOffset = 0;  // Always bind at offset 0 since indirect commands handle offsets
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &miniBatch.m_vertexBuffer, &vertexOffset);

    // Bind the index buffer with the correct offset
    vkCmdBindIndexBuffer(m_commandBuffer, miniBatch.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    VK_BIND_SET_MVP_GRAPHICS(m_commandBuffer, m_graphicsPipelineLayout, 0);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 2, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("SamplerList_ALL"), 0, nullptr);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 3, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ObjectID"), 0, nullptr);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 4, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("DiffuseTextureList"), 0, nullptr);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 5, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ShadowTexture_ALL"), 0, nullptr);

    vkCmdPushConstants(m_commandBuffer, m_graphicsPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(ShaderSetting), &g_ShaderSetting);

    uint32_t drawCount = static_cast<uint32_t>(miniBatch.m_drawIndexedCommands.size());
    vkCmdDrawIndexedIndirect(m_commandBuffer, g_BatchManager.m_indirectDrawBuffer,
                             miniBatch.m_indirectCommandsOffset,   // offset
                             drawCount,                            // drawCount
                             sizeof(VkDrawIndexedIndirectCommand)  // stride
    );
    batchIndex += 1;
    g_ShaderSetting.batchIdx += miniBatch.m_drawIndexedCommands.size();
  }

  ///////////////////////////////////////////
  // BoundingBox Render
  ///////////////////////////////////////////
  g_ShaderSetting.batchIdx = 0;
  if (g_RenderSetting.isRenderBoundingBox) {
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_boundingBoxPipeline);

    for (int i = 0; i < g_BatchManager.m_boundingBoxBufferList.size(); ++i) {
      VkDeviceSize vertexOffset = 0;  // Always bind at offset 0 since indirect commands handle offsets
      vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &g_BatchManager.m_boundingBoxBufferList[i].vertexBuffer, &vertexOffset);

      // Bind the index buffer with the correct offset
      vkCmdBindIndexBuffer(m_commandBuffer, g_BatchManager.m_boundingBoxBufferList[i].indexBuffer, 0, VK_INDEX_TYPE_UINT32);

      VK_BIND_SET_MVP_GRAPHICS(m_commandBuffer, m_graphicsPipelineLayout, 0);
      vkCmdPushConstants(m_commandBuffer, m_graphicsPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(ShaderSetting), &g_ShaderSetting);

      vkCmdDrawIndexed(m_commandBuffer, 24, 1, 0, 0, 0);
      g_ShaderSetting.batchIdx += 1;
    }
  }

  // End Render Pass
  vkCmdEndRenderPass(m_commandBuffer);

  /*
    Dispatch the Ray tracing commands
  */

  // VkUtils::CmdImageBarrier(m_commandBuffer, m_colourBufferImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
  //                          VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  //                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  //                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  // vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_raytracingPipeline);
  // vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_raytracingPipelineLayout, 0, 1,
  //                         &g_DescriptorManager.GetVkDescriptorSet("ViewProjection_ALL"), 0, nullptr);
  // vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_raytracingPipelineLayout, 1, 1,
  // &m_raytracingSet,
  //                         0, nullptr);
  // vkCmdPushConstants(m_commandBuffer, m_raytracingPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(ShaderSetting), &g_ShaderSetting);

  // VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
  // vkCmdTraceRaysKHR(m_commandBuffer, &shaderBindingTables.raygen.stridedDeviceAddressRegion,
  //                   &shaderBindingTables.miss.stridedDeviceAddressRegion, &shaderBindingTables.hit.stridedDeviceAddressRegion,
  //                   &emptySbtEntry, m_width, m_height, 1);

  // VkUtils::CmdImageBarrier(m_commandBuffer, m_colourBufferImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  //                          VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
  //                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  VkUtils::CmdImageBarrier(m_commandBuffer, m_colourBufferImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
  VkUtils::CmdImageBarrier(m_commandBuffer, m_depthStencilBufferImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

  /*
    Object Id commands
  */
  renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.renderPass = m_objectIdRenderPass;        // Render Pass to begin
  renderPassBeginInfo.renderArea.offset = {0, 0};               // Start point of render pass in pixels
  renderPassBeginInfo.renderArea.extent = {m_width, m_height};  // Size of region to run render pass on (starting at offset)

  clearValues = {};
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
  clearValues[1].depthStencil.depth = 1.0f;

  renderPassBeginInfo.pClearValues = clearValues.data();  // List of clear values
  renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

  renderPassBeginInfo.framebuffer = m_objectIdFramebuffer;

  // Begin Render Pass
  vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Bind Pipeline to be used in render pass
  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_objectIDPipeline);
  //
  // mini-batch system
  //
  g_ShaderSetting.batchIdx = 0;
  for (auto& miniBatch : g_BatchManager.m_miniBatchList) {
    // Bind the vertex buffer with the correct offset
    VkDeviceSize vertexOffset = 0;  // Always bind at offset 0 since indirect commands handle offsets
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &miniBatch.m_vertexBuffer, &vertexOffset);

    // Bind the index buffer with the correct offset
    vkCmdBindIndexBuffer(m_commandBuffer, miniBatch.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    VK_BIND_SET_MVP_GRAPHICS(m_commandBuffer, m_graphicsPipelineLayout, 0);

    vkCmdPushConstants(m_commandBuffer, m_graphicsPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(ShaderSetting), &g_ShaderSetting);

    uint32_t drawCount = static_cast<uint32_t>(miniBatch.m_drawIndexedCommands.size());
    vkCmdDrawIndexedIndirect(m_commandBuffer, g_BatchManager.m_indirectDrawBuffer,
                             miniBatch.m_indirectCommandsOffset,   // offset
                             drawCount,                            // drawCount
                             sizeof(VkDrawIndexedIndirectCommand)  // stride
    );
    g_ShaderSetting.batchIdx += miniBatch.m_drawIndexedCommands.size();
  }

  vkCmdEndRenderPass(m_commandBuffer);

  // Stop recording commands to command buffer!
  VK_CHECK(vkEndCommandBuffer(m_commandBuffer));
}
