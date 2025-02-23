#pragma once

#include "BatchSystem.h"
#include "Components.h"
#include "CullingRenderPass.h"
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
  void UpdateTLAS();

  void RebuildAS();

  virtual void Draw(VkSemaphore renderAvailable);

  VkImageView& GetFrameBufferImageView() { return m_colourBufferImageView; };
  VkImageView& GetDepthStencilImageView() { return m_depthStencilBufferImageView; };
  VkSemaphore& GetSemaphore() { return m_renderAvailable; };

 private:
  virtual void CreateRenderPass();
  void CreateLightingRenderPass();
  void CreateObjectIdRenderPass();

  virtual void CreateFramebuffers();
  void CreateLightingFramebuffer();
  void CreateObjectIdFramebuffer();
  void CreateRaytracingFramebuffer();

  virtual void CreatePipelineLayouts();
  void CreateRaytracingPipelineLayout();

  virtual void CreatePipelines();
  void CreateGraphicsPipeline();
  void CreateWireGraphicsPipeline();
  void CreateBoundingBoxPipeline();
  void CreateObjectIDPipeline();
  void CreateRaytracingPipeline();

  virtual void CreateBuffers();
  void CreateLightingPassBuffers();
  void CreateRaytracingBuffers();
  void CreateRaytracingDescriptorSets();
  void CreateBLAS();
  void CreateTLAS();
  void CreateShaderBindingTables();

  void CreatePushConstantRange();

  void CreateSemaphores();
  virtual void CreateCommandBuffers();

  virtual void RecordCommands();
  void RecordLightingPassCommands();
  void RecordRaytracingShadowCommands();
  void RecordBoundingBoxCommands();
  void RecordObjectIDPassCommands();

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
  std::vector<AccelerationStructure> m_bottomLevelASList;
  AccelerationStructure m_topLevelAS;

  std::vector<ScratchBuffer> scratchBuffers;

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
  ScratchBuffer m_scratchBufferTLAS;

  VkDescriptorPool m_raytracingPool;
  VkDescriptorSet m_raytracingSet;
  VkDescriptorSetLayout m_raytracingSetLayout;

  VkImage m_raytracingImage;
  VkDeviceMemory m_raytracingImageMemory;
  VkImageView m_raytracingImageView;

  VkFramebuffer m_framebuffer;
  VkFramebuffer m_objectIdFramebuffer;
  VkFramebuffer m_raytracingFramebuffer;

  VkCommandPool m_pGraphicsCommandPool;
  VkCommandBuffer m_commandBuffer;

  VkPushConstantRange m_debugPushConstant;

  VkSemaphore m_renderAvailable;
  VkFence m_fence;
};
