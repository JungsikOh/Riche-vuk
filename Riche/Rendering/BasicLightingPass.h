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
    // 1. 명령 버퍼 할당
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    // 2. 명령 버퍼 기록 시작 (one-time submit)
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // 3. TLAS 업데이트(Refit) 명령 기록
    VkAccelerationStructureBuildGeometryInfoKHR updateInfo = {};
    updateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    updateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    // 업데이트 플래그 포함
    updateInfo.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    updateInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    // src와 dst에 동일한 TLAS 핸들을 지정하여 업데이트 함
    updateInfo.srcAccelerationStructure = tlas;
    updateInfo.dstAccelerationStructure = tlas;
    updateInfo.geometryCount = 1;
    updateInfo.pGeometries = pGeometry;
    updateInfo.scratchData.deviceAddress = scratchDeviceAddress;

    // Build Range Info: 인스턴스 수만큼 primitiveCount 설정
    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.primitiveCount = numInstances;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &updateInfo, &pRangeInfo);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // 4. fence 생성
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

    // 5. 명령 버퍼 제출
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));

    // 6. 비동기 업데이트: 별도 스레드에서 fence를 기다린 후 cleanup
    std::thread updateThread([=]() {
      // fence가 완료될 때까지 기다림
      VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
      // 완료 후, 커맨드 버퍼와 fence 해제
      vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
      vkDestroyFence(device, fence, nullptr);
    });

    // 스레드를 분리하여 비동기로 실행 (메인 스레드를 block하지 않음)
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
