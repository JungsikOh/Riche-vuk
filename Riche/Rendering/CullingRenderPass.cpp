#include "CullingRenderPass.h"

#include "Camera.h"
#include "Editor/Editor.h"
#include "Mesh.h"
#include "Utils/BoundingBox.h"
#include "VkUtils/ChooseFunc.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/QueueFamilyIndices.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/ShaderModule.h"

void CullingRenderPass::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool,
                                   Camera* camera, const uint32_t width, const uint32_t height) {
  m_pDevice = device;
  m_pPhyscialDevice = physicalDevice;
  m_pGraphicsQueue = queue;

  m_width = width;
  m_height = height;

  m_pCamera = camera;

  m_pGraphicsCommandPool = commandPool;

  std::vector<BasicVertex> allMeshVertices;
  std::vector<uint32_t> allIndices;

  for (int z = -15; z < -5; ++z) {
    for (int y = -5; y < 5; ++y) {
      for (int x = -5; x < 4; ++x) {
        // Create a mesh
        std::vector<BasicVertex> meshVertices = {
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},   {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},     {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},   {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},   {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},   {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},     {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},    {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},    {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},  {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},  {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}};

        std::vector<uint32_t> meshIndices = {0,  3,  2,  2,  1,  0,  4,  7,  6,  6,  5,  4,  8,  11, 10, 10, 9,  8,
                                             12, 15, 14, 14, 13, 12, 16, 19, 18, 18, 17, 16, 20, 23, 22, 22, 21, 20};

        Mesh mesh;
        float posX = x * 1.3f;
        float posY = y * 1.3f;
        float posZ = z * 1.3f;

        mesh.GetModel() = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ));
        m_modelListCPU.push_back(mesh.GetModel());

        mesh.Initialize(meshVertices, meshIndices);
        AddDataToMiniBatch(m_miniBatchList, g_ResourceManager, mesh);

        std::vector<glm::vec3> positions;
        for (const BasicVertex& vertex : meshVertices) {
          positions.push_back(mesh.GetModel() * glm::vec4(vertex.pos, 1.0f));
        }

        AABB aabb = ComputeAABB(positions);
        m_aabbList.push_back(aabb);
      }
    }
  }
  std::vector<BasicVertex> meshVertices = {// Front face
                                           {{-7.5f, -7.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
                                           {{7.5f, -7.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                                           {{7.5f, 7.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                           {{-7.5f, 7.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

                                           // Back face
                                           {{-7.5f, -7.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
                                           {{7.5f, -7.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
                                           {{7.5f, 7.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
                                           {{-7.5f, 7.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

                                           // Left face
                                           {{-7.5f, -7.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                           {{-7.5f, -7.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                                           {{-7.5f, 7.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                                           {{-7.5f, 7.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

                                           // Right face
                                           {{7.5f, -7.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                           {{7.5f, -7.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                                           {{7.5f, 7.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                                           {{7.5f, 7.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

                                           // Top face
                                           {{-7.5f, 7.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                                           {{7.5f, 7.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                           {{7.5f, 7.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
                                           {{-7.5f, 7.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

                                           // Bottom face
                                           {{-7.5f, -7.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                                           {{7.5f, -7.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
                                           {{7.5f, -7.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
                                           {{-7.5f, -7.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}};

  std::vector<uint32_t> meshIndices = {
      0,  3,  2,  2,  1,  0,   // Front face
      4,  7,  6,  6,  5,  4,   // Back face
      8,  11, 10, 10, 9,  8,   // Left face
      12, 15, 14, 14, 13, 12,  // Right face
      16, 19, 18, 18, 17, 16,  // Top face
      20, 23, 22, 22, 21, 20   // Bottom face
  };
  Mesh mesh;
  mesh.GetModel() = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
  m_modelListCPU.push_back(mesh.GetModel());

  mesh.Initialize(meshVertices, meshIndices);
  AddDataToMiniBatch(m_miniBatchList, g_ResourceManager, mesh);

  std::vector<glm::vec3> positions;
  for (const BasicVertex& vertex : meshVertices) {
    positions.push_back(mesh.GetModel() * glm::vec4(vertex.pos, 1.0f));
  }

  AABB aabb = ComputeAABB(positions);
  m_aabbList.push_back(aabb);
  FlushMiniBatch(m_miniBatchList, g_ResourceManager);

  CreateRenderPass();
  CreateDepthRenderPass();

  CreateFramebuffer();
  CreateDepthFramebuffer();

  CreateBuffers();
  CreateDesrciptorSets();
  CreatePushConstantRange();

  CreatePipeline();

  CreateSemaphores();
  CreateCommandBuffers();
}

void CullingRenderPass::Cleanup() {
  for (auto batch : m_miniBatchList) {
    vkDestroyBuffer(m_pDevice, batch.m_vertexBuffer, nullptr);
    vkFreeMemory(m_pDevice, batch.m_vertexBufferMemory, nullptr);

    vkDestroyBuffer(m_pDevice, batch.m_indexBuffer, nullptr);
    vkFreeMemory(m_pDevice, batch.m_indexBufferMemory, nullptr);
  }

  // Basic
  vkDestroyImageView(m_pDevice, m_colourBufferImageView, nullptr);
  vkDestroyImage(m_pDevice, m_colourBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_colourBufferImageMemory, nullptr);

  vkDestroyImageView(m_pDevice, m_depthStencilBufferImageView, nullptr);
  vkDestroyImage(m_pDevice, m_depthStencilBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_depthStencilBufferImageMemory, nullptr);

  vkDestroyFramebuffer(m_pDevice, m_framebuffer, nullptr);

  vkDestroyBuffer(m_pDevice, m_viewProjectionUBO, nullptr);
  vkFreeMemory(m_pDevice, m_viewProjectionUBOMemory, nullptr);

  vkDestroyBuffer(m_pDevice, m_indirectDrawBuffer, nullptr);
  vkFreeMemory(m_pDevice, m_indirectDrawBufferMemory, nullptr);

  vkDestroyBuffer(m_pDevice, m_modelListUBO, nullptr);
  vkFreeMemory(m_pDevice, m_modelListUBOMemory, nullptr);

  vkDestroyBuffer(m_pDevice, m_aabbListBuffer, nullptr);
  vkFreeMemory(m_pDevice, m_aabbListBufferMemory, nullptr);

  vkDestroyBuffer(m_pDevice, m_cameraFrustumBuffer, nullptr);
  vkFreeMemory(m_pDevice, m_cameraFrustumBufferMemory, nullptr);

  vkDestroySemaphore(m_pDevice, m_renderAvailable, nullptr);
  vkDestroyFence(m_pDevice, m_fence, nullptr);
  vkFreeCommandBuffers(m_pDevice, m_pGraphicsCommandPool, 1, &m_commandBuffer);

  vkDestroyPipeline(m_pDevice, m_graphicsPipeline, nullptr);
  vkDestroyPipeline(m_pDevice, m_wireGraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_pDevice, m_graphicsPipelineLayout, nullptr);
  vkDestroyRenderPass(m_pDevice, m_renderPass, nullptr);

  // Compute
  vkDestroyPipeline(m_pDevice, m_viewCullingComputePipeline, nullptr);
  vkDestroyPipelineLayout(m_pDevice, m_viewCullingComputePipelineLayout, nullptr);

  vkDestroyPipeline(m_pDevice, m_occlusionCullingComputePipeline, nullptr);
  vkDestroyPipelineLayout(m_pDevice, m_occlusionCullingComputePipelineLayout, nullptr);

#ifdef _DEBUG
  vkDestroyBuffer(m_pDevice, m_fLODListBuffer, nullptr);
  vkFreeMemory(m_pDevice, m_fLODListBufferMemory, nullptr);
#endif  // _DEBUG

  // DepthOnly
  for (int i = 0; i < HIZ_MIP_LEVEL; ++i) {
    vkDestroyFramebuffer(m_pDevice, m_depthFramebuffers[i], nullptr);
    vkDestroyImageView(m_pDevice, m_onlyDepthBufferImageViews[i], nullptr);
  }
  vkDestroyImageView(m_pDevice, m_onlyDepthBufferImageViews[HIZ_MIP_LEVEL], nullptr);
  vkDestroyImage(m_pDevice, m_onlyDepthBufferImage, nullptr);
  vkFreeMemory(m_pDevice, m_onlyDepthBufferImageMemory, nullptr);

  vkDestroyPipeline(m_pDevice, m_depthGraphicePipeline, nullptr);
  vkDestroyRenderPass(m_pDevice, m_depthRenderPass, nullptr);
}

void CullingRenderPass::Update() {
  m_viewProjectionCPU.view = m_pCamera->View();
  m_viewProjectionCPU.projection = m_pCamera->Proj();

  void* pData = nullptr;
  vkMapMemory(m_pDevice, m_modelListUBOMemory, 0, sizeof(glm::mat4) * m_modelListCPU.size(), 0, &pData);
  memcpy(pData, m_modelListCPU.data(), sizeof(glm::mat4) * m_modelListCPU.size());
  vkUnmapMemory(m_pDevice, m_modelListUBOMemory);

  vkMapMemory(m_pDevice, m_viewProjectionUBOMemory, 0, sizeof(ViewProjection), 0, &pData);
  memcpy(pData, &m_viewProjectionCPU, sizeof(ViewProjection));
  vkUnmapMemory(m_pDevice, m_viewProjectionUBOMemory);

  VkDeviceSize aabbBufferSize = sizeof(AABB) * m_aabbList.size();
  vkMapMemory(m_pDevice, m_aabbListBufferMemory, 0, aabbBufferSize, 0, &pData);
  memcpy(pData, m_aabbList.data(), aabbBufferSize);
  vkUnmapMemory(m_pDevice, m_aabbListBufferMemory);

  std::array<FrustumPlane, 6> cameraFrustumPlanes = CalculateFrustumPlanes(m_pCamera->ViewProj());
  VkDeviceSize cameraPlaneSize = sizeof(FrustumPlane) * cameraFrustumPlanes.size();
  vkMapMemory(m_pDevice, m_cameraFrustumBufferMemory, 0, cameraPlaneSize, 0, &pData);
  memcpy(pData, cameraFrustumPlanes.data(), cameraPlaneSize);
  vkUnmapMemory(m_pDevice, m_cameraFrustumBufferMemory);
}

void CullingRenderPass::Draw(VkSemaphore renderAvailable) {
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

void CullingRenderPass::CreateRenderPass() {
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
}

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

void CullingRenderPass::CreateFramebuffer() {
  VkFormat colourImageFormat = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_R8G8B8A8_UNORM}, VK_IMAGE_TILING_OPTIMAL,
                                                              VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_colourBufferImageMemory, &m_colourBufferImage,
                         colourImageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
  VkUtils::CreateImageView(m_pDevice, m_colourBufferImage, &m_colourBufferImageView, colourImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

  VkFormat depthImageFormat = VkUtils::ChooseSupportedFormat(
      m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_depthStencilBufferImageMemory, &m_depthStencilBufferImage,
                         depthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
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
}

void CullingRenderPass::CreateDepthFramebuffer() {
  VkFormat depthImageFormat = VkUtils::ChooseSupportedFormat(m_pPhyscialDevice, {VK_FORMAT_D32_SFLOAT}, VK_IMAGE_TILING_OPTIMAL,
                                                             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkUtils::CreateImage2D(m_pDevice, m_pPhyscialDevice, m_width, m_height, &m_onlyDepthBufferImageMemory, &m_onlyDepthBufferImage,
                         depthImageFormat,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr, HIZ_MIP_LEVEL);

  m_onlyDepthBufferImageViews.resize(HIZ_MIP_LEVEL + 1);
  m_depthFramebuffers.resize(HIZ_MIP_LEVEL);
  for (int i = 0; i < HIZ_MIP_LEVEL; ++i) {
    VkUtils::CreateImageView(m_pDevice, m_onlyDepthBufferImage, &m_onlyDepthBufferImageViews[i], depthImageFormat,
                             VK_IMAGE_ASPECT_DEPTH_BIT, i, 1);

    std::array<VkImageView, 1> attachments = {m_onlyDepthBufferImageViews[i]};

    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = m_depthRenderPass;  // Render pass layout the framebuffer will be used with
    framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferCreateInfo.pAttachments = attachments.data();     // List of attachments (1:1 with render pass)
    framebufferCreateInfo.width = std::max(m_width >> i, 1u);    // framebuffer width
    framebufferCreateInfo.height = std::max(m_height >> i, 1u);  // framebuffer height
    framebufferCreateInfo.layers = 1;                            // framebuffer layers

    VK_CHECK(vkCreateFramebuffer(m_pDevice, &framebufferCreateInfo, nullptr, &m_depthFramebuffers[i]));
  }
  VkUtils::CreateImageView(m_pDevice, m_onlyDepthBufferImage, &m_onlyDepthBufferImageViews[HIZ_MIP_LEVEL], depthImageFormat,
                           VK_IMAGE_ASPECT_DEPTH_BIT, 0, HIZ_MIP_LEVEL);
}

void CullingRenderPass::CreatePipeline() {
  CraeteGrahpicsPipeline();
  CreateWireGraphicsPipeline();
  CreateDepthGraphicsPipeline();
  CraeteViewCullingComputePipeline();
  CreateOcclusionCullingComputePipeline();
}

void CullingRenderPass::CraeteGrahpicsPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/vert.spv");
  auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/frag.spv");

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
  attributeDescriptions[1].binding = 0;                          // Which binding the data is at (should be same as above)
  attributeDescriptions[1].location = 1;                         // Location in shader where data will be read from
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  // Format the data will take (also helps define size of data)
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
  VkDescriptorSetLayout setLayouts[2] = {g_DescriptorManager.GetVkDescriptorSetLayout("ViewProjection"),
                                         g_DescriptorManager.GetVkDescriptorSetLayout("ModelList")};

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 2;
  pipelineLayoutCreateInfo.pSetLayouts = setLayouts;

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

void CullingRenderPass::CreateWireGraphicsPipeline() {
  auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/vert.spv");
  auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/frag.spv");

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
  attributeDescriptions[1].binding = 0;                          // Which binding the data is at (should be same as above)
  attributeDescriptions[1].location = 1;                         // Location in shader where data will be read from
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  // Format the data will take (also helps define size of data)
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
  attributeDescriptions[1].binding = 0;                          // Which binding the data is at (should be same as above)
  attributeDescriptions[1].location = 1;                         // Location in shader where data will be read from
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  // Format the data will take (also helps define size of data)
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

void CullingRenderPass::CraeteViewCullingComputePipeline() {
  auto computeShaderCode = VkUtils::ReadFile("Resources/Shaders/viewfrustumculling_comp.spv");
  // Build Shaders
  VkShaderModule computeShaderModule = VkUtils::CreateShaderModule(m_pDevice, computeShaderCode);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &g_DescriptorManager.GetVkDescriptorSetLayout("ViewFrustumCulling_COMPUTE");

  VK_CHECK(vkCreatePipelineLayout(m_pDevice, &pipelineLayoutInfo, nullptr, &m_viewCullingComputePipelineLayout));

  VkComputePipelineCreateInfo computePipelineCreateInfo = {};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  computePipelineCreateInfo.stage.module = computeShaderModule;
  computePipelineCreateInfo.stage.pName = "main";
  computePipelineCreateInfo.layout = m_viewCullingComputePipelineLayout;

  VK_CHECK(vkCreateComputePipelines(m_pDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_viewCullingComputePipeline));

  vkDestroyShaderModule(m_pDevice, computeShaderModule, nullptr);
}

void CullingRenderPass::CreateOcclusionCullingComputePipeline() {
  auto computeShaderCode = VkUtils::ReadFile("Resources/Shaders/HiZOcclusionCullingCS.spv");
  // Build Shaders
  VkShaderModule computeShaderModule = VkUtils::CreateShaderModule(m_pDevice, computeShaderCode);

  std::array<VkDescriptorSetLayout, 3> setLayouts = {g_DescriptorManager.GetVkDescriptorSetLayout("SamplerList_ALL"),
                                                     g_DescriptorManager.GetVkDescriptorSetLayout("ViewFrustumCulling_COMPUTE"),
                                                     g_DescriptorManager.GetVkDescriptorSetLayout("DepthOnlyImage")};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = setLayouts.size();
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &m_debugPushConstant;

  VK_CHECK(vkCreatePipelineLayout(m_pDevice, &pipelineLayoutInfo, nullptr, &m_occlusionCullingComputePipelineLayout));

  VkComputePipelineCreateInfo computePipelineCreateInfo = {};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  computePipelineCreateInfo.stage.module = computeShaderModule;
  computePipelineCreateInfo.stage.pName = "main";
  computePipelineCreateInfo.layout = m_occlusionCullingComputePipelineLayout;

  VK_CHECK(
      vkCreateComputePipelines(m_pDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_occlusionCullingComputePipeline));

  vkDestroyShaderModule(m_pDevice, computeShaderModule, nullptr);
}

void CullingRenderPass::CreateBuffers() {
  CreateUniformBuffers();
  CreateShaderStorageBuffers();
}

void CullingRenderPass::CreateShaderStorageBuffers() {
  void* pData = nullptr;

  VkDeviceSize indirectBufferSize = 0;
  std::vector<VkDrawIndexedIndirectCommand> flattenCommands;
  for (int i = 0; i < m_miniBatchList.size(); ++i) {
    indirectBufferSize += sizeof(VkDrawIndexedIndirectCommand) * m_miniBatchList[i].m_drawIndexedCommands.size();
    flattenCommands.insert(flattenCommands.end(), m_miniBatchList[i].m_drawIndexedCommands.begin(),
                           m_miniBatchList[i].m_drawIndexedCommands.end());
  }
  VkUtils::CreateBuffer(
      m_pDevice, m_pPhyscialDevice, indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_indirectDrawBuffer, &m_indirectDrawBufferMemory);

  vkMapMemory(m_pDevice, m_indirectDrawBufferMemory, 0, indirectBufferSize, 0, &pData);
  memcpy(pData, flattenCommands.data(), indirectBufferSize);
  vkUnmapMemory(m_pDevice, m_indirectDrawBufferMemory);

  VkDeviceSize aabbBufferSize = sizeof(AABB) * m_aabbList.size();
  VkUtils::CreateBuffer(
      m_pDevice, m_pPhyscialDevice, aabbBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_aabbListBuffer, &m_aabbListBufferMemory);

  vkMapMemory(m_pDevice, m_aabbListBufferMemory, 0, aabbBufferSize, 0, &pData);
  memcpy(pData, m_aabbList.data(), aabbBufferSize);
  vkUnmapMemory(m_pDevice, m_aabbListBufferMemory);

  std::array<FrustumPlane, 6> cameraFrustumPlanes = CalculateFrustumPlanes(m_pCamera->ViewProj());
  VkDeviceSize cameraPlaneSize = sizeof(FrustumPlane) * cameraFrustumPlanes.size();
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, cameraPlaneSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_cameraFrustumBuffer,
                        &m_cameraFrustumBufferMemory);

  vkMapMemory(m_pDevice, m_cameraFrustumBufferMemory, 0, cameraPlaneSize, 0, &pData);
  memcpy(pData, cameraFrustumPlanes.data(), cameraPlaneSize);
  vkUnmapMemory(m_pDevice, m_cameraFrustumBufferMemory);

  VkDescriptorBufferInfo indirectBufferInfo = {};
  indirectBufferInfo.buffer = m_indirectDrawBuffer;  // Buffer to get data from
  indirectBufferInfo.offset = 0;                     // Position of start of data
  indirectBufferInfo.range = indirectBufferSize;     // size of data

  VkDescriptorBufferInfo aabbIndirectInfo = {};
  aabbIndirectInfo.buffer = m_aabbListBuffer;  // Buffer to get data from
  aabbIndirectInfo.offset = 0;                 // Position of start of data
  aabbIndirectInfo.range = aabbBufferSize;     // size of data

  VkDescriptorBufferInfo cameraBoundingBoxInfo = {};
  cameraBoundingBoxInfo.buffer = m_cameraFrustumBuffer;  // Buffer to get data from
  cameraBoundingBoxInfo.offset = 0;                      // Position of start of data
  cameraBoundingBoxInfo.range = cameraPlaneSize;         // size of data

  VkDescriptorBufferInfo viewProjectionBufferInfo = {};
  viewProjectionBufferInfo.buffer = m_viewProjectionUBO;    // Buffer to get data from
  viewProjectionBufferInfo.offset = 0;                      // Position of start of data
  viewProjectionBufferInfo.range = sizeof(ViewProjection);  // size of data

  VkUtils::DescriptorBuilder indirect = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  indirect.BindBuffer(0, &indirectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
  indirect.BindBuffer(1, &aabbIndirectInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
  indirect.BindBuffer(2, &cameraBoundingBoxInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
  indirect.BindBuffer(3, &viewProjectionBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

#ifdef _DEBUG  // NDEBUG is C++ standard Macro.
  VkDeviceSize fLODListSize = sizeof(float) * m_aabbList.size();
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, fLODListSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_fLODListBuffer,
                        &m_fLODListBufferMemory);

  VkDescriptorBufferInfo fLODListBufferInfo = {};
  fLODListBufferInfo.buffer = m_fLODListBuffer;  // Buffer to get data from
  fLODListBufferInfo.offset = 0;                 // Position of start of data
  fLODListBufferInfo.range = fLODListSize;       // size of data

  indirect.BindBuffer(4, &fLODListBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
#endif

  g_DescriptorManager.AddDescriptorSet(&indirect, "ViewFrustumCulling_COMPUTE");
}

void CullingRenderPass::CreateUniformBuffers() {
  VkDeviceSize vpBufferSize = sizeof(ViewProjection);
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_viewProjectionUBO,
                        &m_viewProjectionUBOMemory);

  VkDeviceSize modelBufferSize = sizeof(glm::mat4) * m_modelListCPU.size();
  VkUtils::CreateBuffer(m_pDevice, m_pPhyscialDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_modelListUBO,
                        &m_modelListUBOMemory);
}

void CullingRenderPass::CreateDesrciptorSets() {
  // VIEW PROJECTION DESCRIPTOR
  // Buffer Info and data offset info
  VkDescriptorBufferInfo uboViewProjectionBufferInfo = {};
  uboViewProjectionBufferInfo.buffer = m_viewProjectionUBO;    // Buffer to get data from
  uboViewProjectionBufferInfo.offset = 0;                      // Position of start of data
  uboViewProjectionBufferInfo.range = sizeof(ViewProjection);  // size of data

  VkUtils::DescriptorBuilder vpBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  vpBuilder.BindBuffer(0, &uboViewProjectionBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
  g_DescriptorManager.AddDescriptorSet(&vpBuilder, "ViewProjection");

  VkDescriptorBufferInfo modelUBOInfo = {};
  modelUBOInfo.buffer = m_modelListUBO;                            // Buffer to get data from
  modelUBOInfo.offset = 0;                                         // Position of start of data
  modelUBOInfo.range = sizeof(glm::mat4) * m_modelListCPU.size();  // size of data

  VkUtils::DescriptorBuilder modelBulder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  modelBulder.BindBuffer(0, &modelUBOInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
  g_DescriptorManager.AddDescriptorSet(&modelBulder, "ModelList");

  VkUtils::DescriptorBuilder depthOnlyImageBuilder =
      VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  for (int i = 0; i < 1; ++i) {
    VkDescriptorImageInfo depthOnlyImageInfo = {};
    depthOnlyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthOnlyImageInfo.imageView = m_onlyDepthBufferImageViews[HIZ_MIP_LEVEL];
    depthOnlyImageInfo.sampler = VK_NULL_HANDLE;

    depthOnlyImageBuilder.BindImage(i, &depthOnlyImageInfo, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  g_DescriptorManager.AddDescriptorSet(&depthOnlyImageBuilder, "DepthOnlyImage");
}

void CullingRenderPass::CreatePushConstantRange() {
  m_debugPushConstant.stageFlags = VK_SHADER_STAGE_ALL;                 // Shader stage push constant will go to
  m_debugPushConstant.offset = 0;                               // Offset into given data to pass to push constant
  m_debugPushConstant.size = sizeof(ShaderSetting);                     // Size of Data Being Passed
}

void CullingRenderPass::CreateSemaphores() {
  // Semaphore creataion information
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VK_CHECK(vkCreateFence(m_pDevice, &fenceCreateInfo, nullptr, &m_fence));
  VK_CHECK(vkCreateSemaphore(m_pDevice, &semaphoreCreateInfo, nullptr, &m_renderAvailable));
}

void CullingRenderPass::CreateCommandBuffers() {
  //
  // Single Command Buffer
  //
  VkCommandBufferAllocateInfo cbAllocInfo = {};
  cbAllocInfo = {};
  cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAllocInfo.commandPool = m_pGraphicsCommandPool;  // 해당 큐 패밀리의 큐에서만 커맨드 큐 동작이 실행가능하다.
  cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to
                                                        // queue. cant be called by other buffers
  // VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via 'vkCmdExecuteCommands'
  // when recording commands in primary buffer.
  cbAllocInfo.commandBufferCount = 1;

  // Allocate command buffers and place handles in array of buffers
  VK_CHECK(vkAllocateCommandBuffers(m_pDevice, &cbAllocInfo, &m_commandBuffer));
}

void CullingRenderPass::RecordCommands() {
  // information about how to begin each command buffer
  VkCommandBufferBeginInfo bufferBeginInfo = {};
  bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;  // Buffer can be resubmitted when it has already been
                                                                         // submitted and is awaiting executi

  // Start recording commands to command buffer!
  VkResult result = vkBeginCommandBuffer(m_commandBuffer, &bufferBeginInfo);
  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to start recording a Command Buffer!");
  }

  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_viewCullingComputePipeline);
  vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_viewCullingComputePipelineLayout, 0, 1,
                          &g_DescriptorManager.GetVkDescriptorSet("ViewFrustumCulling_COMPUTE"), 0, nullptr);

  vkCmdDispatch(m_commandBuffer, 1000, 1, 1);

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
  depthOnlyRenderPassBeginInfo.framebuffer = m_depthFramebuffers[0];

  // Begin Render Pass
  vkCmdBeginRenderPass(m_commandBuffer, &depthOnlyRenderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);  // 렌더 패스의 내용을 직접 명령 버퍼에 기록하는 것을 의미

  // Bind Pipeline to be used in render pass
  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthGraphicePipeline);

  //
  // mini-batch system
  //
  for (auto miniBatch : m_miniBatchList) {
    // Bind the vertex buffer with the correct offset
    VkDeviceSize vertexOffset = 0;  // Always bind at offset 0 since indirect commands handle offsets
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &miniBatch.m_vertexBuffer, &vertexOffset);

    // Bind the index buffer with the correct offset
    vkCmdBindIndexBuffer(m_commandBuffer, miniBatch.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ViewProjection"), 0, nullptr);

    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 1, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ModelList"), 0, nullptr);

    uint32_t drawCount = static_cast<uint32_t>(miniBatch.m_drawIndexedCommands.size());
    vkCmdDrawIndexedIndirect(m_commandBuffer, m_indirectDrawBuffer,
                             0,                                    // offset
                             drawCount,                            // drawCount
                             sizeof(VkDrawIndexedIndirectCommand)  // stride
    );
  }
  vkCmdEndRenderPass(m_commandBuffer);

  if (g_RenderSetting.isOcclusionCulling) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_onlyDepthBufferImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    // 8.3. Blit to generate Mip Levels
    for (uint32_t i = 1; i < HIZ_MIP_LEVEL; ++i) {
      // Define source and destination Mip Levels
      uint32_t srcMip = i - 1;
      uint32_t dstMip = i;

      // Define source and destination regions
      VkImageBlit blit = {};
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      blit.srcSubresource.mipLevel = srcMip;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = 1;
      blit.srcOffsets[0] = {0, 0, 0};
      blit.srcOffsets[1] = {static_cast<int32_t>(std::max(m_width >> srcMip, 1u)),
                            static_cast<int32_t>(std::max(m_height >> srcMip, 1u)), 1};

      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      blit.dstSubresource.mipLevel = dstMip;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount = 1;
      blit.dstOffsets[0] = {0, 0, 0};
      blit.dstOffsets[1] = {static_cast<int32_t>(std::max(m_width >> dstMip, 1u)),
                            static_cast<int32_t>(std::max(m_height >> dstMip, 1u)), 1};

      // Transition destination Mip Level to TRANSFER_DST_OPTIMAL
      VkImageMemoryBarrier dstBarrier = {};
      dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      dstBarrier.image = m_onlyDepthBufferImage;
      dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      dstBarrier.subresourceRange.baseMipLevel = dstMip;
      dstBarrier.subresourceRange.levelCount = 1;
      dstBarrier.subresourceRange.baseArrayLayer = 0;
      dstBarrier.subresourceRange.layerCount = 1;
      dstBarrier.srcAccessMask = 0;
      dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                           1, &dstBarrier);

      // Perform the blit
      vkCmdBlitImage(m_commandBuffer, m_onlyDepthBufferImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_onlyDepthBufferImage,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

      // Transition destination Mip Level to TRANSFER_SRC_OPTIMAL for next Blit
      VkImageMemoryBarrier srcBarrier = {};
      srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      srcBarrier.image = m_onlyDepthBufferImage;
      srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      srcBarrier.subresourceRange.baseMipLevel = dstMip;
      srcBarrier.subresourceRange.levelCount = 1;
      srcBarrier.subresourceRange.baseArrayLayer = 0;
      srcBarrier.subresourceRange.layerCount = 1;
      srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

      vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                           1, &srcBarrier);
    }

    // 8.4. Transition all Mip Levels to SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier finalBarrier = {};
    finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.image = m_onlyDepthBufferImage;
    finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    finalBarrier.subresourceRange.baseMipLevel = 0;
    finalBarrier.subresourceRange.levelCount = HIZ_MIP_LEVEL;
    finalBarrier.subresourceRange.baseArrayLayer = 0;
    finalBarrier.subresourceRange.layerCount = 1;
    finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 또는 필요한 셰이더 스테이지
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    //
    // Hi-Z Occlusion Culling
    //
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_occlusionCullingComputePipeline);
    vkCmdPushConstants(m_commandBuffer, m_occlusionCullingComputePipelineLayout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(ShaderSetting), &g_ShaderSetting);

    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_occlusionCullingComputePipelineLayout, 0, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("SamplerList_ALL"), 0, nullptr);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_occlusionCullingComputePipelineLayout, 1, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ViewFrustumCulling_COMPUTE"), 0, nullptr);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_occlusionCullingComputePipelineLayout, 2, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("DepthOnlyImage"), 0, nullptr);

    vkCmdDispatch(m_commandBuffer, 1000, 1, 1);
  }

  // Information about how to begin a render pass (only needed for graphical applications)
  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.renderPass = m_renderPass;                // Render Pass to begin
  renderPassBeginInfo.renderArea.offset = {0, 0};               // Start point of render pass in pixels
  renderPassBeginInfo.renderArea.extent = {m_width, m_height};  // Size of region to run render pass on (starting at offset)

  std::array<VkClearValue, 2> clearValues = {};
  clearValues[0].color = {0.0f, 1.0f, 1.0f, 1.0f};
  clearValues[1].depthStencil.depth = 1.0f;

  renderPassBeginInfo.pClearValues = clearValues.data();  // List of clear values
  renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

  renderPassBeginInfo.framebuffer = m_framebuffer;

  // Begin Render Pass
  vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);  // 렌더 패스의 내용을 직접 명령 버퍼에 기록하는 것을 의미

  // Bind Pipeline to be used in render pass
  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    g_RenderSetting.isWireRendering ? m_wireGraphicsPipeline : m_graphicsPipeline);

  //
  // mini-batch system
  //
  for (auto miniBatch : m_miniBatchList) {
    // Bind the vertex buffer with the correct offset
    VkDeviceSize vertexOffset = 0;  // Always bind at offset 0 since indirect commands handle offsets
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &miniBatch.m_vertexBuffer, &vertexOffset);

    // Bind the index buffer with the correct offset
    vkCmdBindIndexBuffer(m_commandBuffer, miniBatch.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ViewProjection"), 0, nullptr);

    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 1, 1,
                            &g_DescriptorManager.GetVkDescriptorSet("ModelList"), 0, nullptr);

    uint32_t drawCount = static_cast<uint32_t>(miniBatch.m_drawIndexedCommands.size());
    vkCmdDrawIndexedIndirect(m_commandBuffer, m_indirectDrawBuffer,
                             0,                                    // offset
                             drawCount,                            // drawCount
                             sizeof(VkDrawIndexedIndirectCommand)  // stride
    );
  }

  // End Render Pass
  vkCmdEndRenderPass(m_commandBuffer);

  VkUtils::CmdImageBarrier(m_commandBuffer, m_colourBufferImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
  VkUtils::CmdImageBarrier(m_commandBuffer, m_depthStencilBufferImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

  // Stop recording commands to command buffer!
  VK_CHECK(vkEndCommandBuffer(m_commandBuffer));
}
