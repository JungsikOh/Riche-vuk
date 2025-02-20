#pragma once

#include "BatchSystem.h"
#include "Camera.h"
#include "Components.h"
#include "CullingRenderPass.h"
#include "Editor/Editor.h"
#include "IRenderPass.h"
#include "Utils/ThreadPool.h"
#include "VkUtils/ChooseFunc.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/ShaderModule.h"
#include "VulkanRenderer.h"

class Camera;
class Editor;
class BasicLightingPass : public IRenderPass {
 public:
  BasicLightingPass() = default;
  BasicLightingPass(VkDevice device, VkPhysicalDevice physicalDevice);
  ~BasicLightingPass() = default;

  virtual void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, Camera* camera,
                          Editor* editor, const uint32_t width, const uint32_t height);
  virtual void Cleanup();

  virtual void Update();

  virtual void Draw(VkSemaphore renderAvailable);

  VkImageView& GetFrameBufferImageView() { return m_colourBufferImageView; };
  VkImageView& GetDepthStencilImageView() { return m_depthStencilBufferImageView; };
  VkSemaphore& GetSemaphore() { return m_renderAvailable; };

 private:
  // - Rendering Pipeline
  virtual void CreateRenderPass();
  void CreateObjectIdRenderPass();
  void CreateRaytracingRenderPass();

  virtual void CreateFramebuffer();
  void CreateObjectIdFramebuffer();
  void CreateRaytracingFramebuffer();

  virtual void CreatePipelineLayout();
  void CreateRaytracingPipelineLayout();

  virtual void CreatePipeline();
  void CraeteGraphicsPipeline();
  void CreateWireGraphicsPipeline();
  void CreateBoundingBoxPipeline();
  void CreateObjectIDPipeline();
  void CreateRaytracingPipeline();

  virtual void CreateBuffers();
  void CreateBindlessResources();
  void CreateRaytracingBuffers();
  void CreateRaytracingDescriptorSets();
  void CreateBLAS();
  void CreateTLAS();
  void CreateShaderBindingTables();

  void UpdateTLASAsync(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkAccelerationStructureKHR tlas,
                       VkAccelerationStructureGeometryKHR* pGeometry, uint32_t numInstances, VkDeviceAddress scratchDeviceAddress) {
    // 1. ��� ���� �Ҵ�
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    // 2. ��� ���� ��� ���� (one-time submit)
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // 3. TLAS ������Ʈ(Refit) ��� ���
    VkAccelerationStructureBuildGeometryInfoKHR updateInfo = {};
    updateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    updateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    // ������Ʈ �÷��� ����
    updateInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    updateInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    // src�� dst�� ������ TLAS �ڵ��� �����Ͽ� ������Ʈ ��
    updateInfo.srcAccelerationStructure = tlas;
    updateInfo.dstAccelerationStructure = tlas;
    updateInfo.geometryCount = 1;
    updateInfo.pGeometries = pGeometry;
    updateInfo.scratchData.deviceAddress = scratchDeviceAddress;

    // Build Range Info: �ν��Ͻ� ����ŭ primitiveCount ����
    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.primitiveCount = numInstances;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &updateInfo, &pRangeInfo);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // 4. fence ����
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

    // 5. ��� ���� ����
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));

    // 6. �񵿱� ������Ʈ: ���� �����忡�� fence�� ��ٸ� �� cleanup
    std::thread updateThread([=]() {
      // fence�� �Ϸ�� ������ ��ٸ�
      VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
      // �Ϸ� ��, Ŀ�ǵ� ���ۿ� fence ����
      vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
      vkDestroyFence(device, fence, nullptr);
    });

    // �����带 �и��Ͽ� �񵿱�� ���� (���� �����带 block���� ����)
    updateThread.detach();
  }

  void CreatePushConstantRange();

  void CreateSemaphores();
  virtual void CreateCommandBuffers();
  virtual void RecordCommands();

 private:
  Editor* m_pEditor;

  // - Main Objects
  VkDevice m_pDevice;
  VkPhysicalDevice m_pPhyscialDevice;

  VkQueue m_pGraphicsQueue;

  uint32_t m_width;
  uint32_t m_height;
  Camera* m_pCamera;

  // - Rendering Graphics Pipeline
  VkRenderPass m_renderPass;
  VkRenderPass m_objectIdRenderPass;
  VkRenderPass m_raytracingRenderPass;

  VkPipeline m_graphicsPipeline;
  VkPipeline m_wireGraphicsPipeline;  // Debugging (wireframe)
  VkPipeline m_boundingBoxPipeline;
  VkPipeline m_objectIDPipeline;
  VkPipelineLayout m_graphicsPipelineLayout;

  VkImage m_colourBufferImage;
  VkDeviceMemory m_colourBufferImageMemory;
  VkImageView m_colourBufferImageView;

  VkImage m_depthStencilBufferImage;
  VkDeviceMemory m_depthStencilBufferImageMemory;
  VkImageView m_depthStencilBufferImageView;

  // For ObjectID
  VkImage m_objectIdColourBufferImage;
  VkDeviceMemory m_objectIdBufferImageMemory;
  VkImageView m_objectIdBufferImageView;

  VkImage m_objectIdDepthStencilBufferImage;
  VkDeviceMemory m_objectIdDepthStencilBufferImageMemory;
  VkImageView m_objectIdDepthStencilBufferImageView;

  // For Raytracing
  AccelerationStructure m_bottomLevelAS;
  std::vector<AccelerationStructure> m_bottomLevelASList; 
  AccelerationStructure m_topLevelAS;

    std::vector<ScratchBuffer> scratchBuffers;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;
  std::vector<VkAccelerationStructureBuildGeometryInfoKHR> accelerationBuildGeometryInfos;

  std::vector<InstanceOffset> m_instanceOffsets;
  VkBuffer m_instanceOffsetBuffer;
  VkDeviceMemory m_instanceOffsetBufferMemory;

  VkPipeline m_raytracingPipeline;
  VkPipelineLayout m_raytracingPipelineLayout;

  std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

  struct ShaderBindingTables {
    ShaderBindingTable raygen;
    ShaderBindingTable miss;
    ShaderBindingTable hit;
  } shaderBindingTables;

  VkBuffer instanceBuffer;
  VkDeviceMemory instanceBufferMemory;
  ScratchBuffer scratchBufferTLAS;

  VkDescriptorPool m_raytracingPool;
  VkDescriptorSet m_raytracingSet;
  VkDescriptorSetLayout m_raytracingSetLayout;

  VkImage m_raytracingImage;
  VkDeviceMemory m_raytracingImageMemory;
  VkImageView m_raytracingImageView;

  VkImage m_rayDepthStencilBufferImage;
  VkDeviceMemory m_rayDepthStencilBufferImageMemory;
  VkImageView m_rayDepthStencilBufferImageView;

  VkFramebuffer m_framebuffer;
  VkFramebuffer m_objectIdFramebuffer;
  VkFramebuffer m_raytracingFramebuffer;

  VkCommandPool m_pGraphicsCommandPool;
  VkCommandBuffer m_commandBuffer;

  VkPushConstantRange m_debugPushConstant;

  VkSemaphore m_renderAvailable;
  VkFence m_fence;
};
