#pragma once

#include "BatchSystem.h"
#include "Components.h"
#include "CullingRenderPass.h"
#include "IRenderPass.h"
#include "Image.h"
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

  virtual void Draw(uint32_t imageIndex, VkSemaphore renderAvailable);

  VkImageView& GetFrameBufferImageView(uint32_t imageIndex) { return m_colourBufferImages[imageIndex].imageView; };
  VkImageView& GetDepthStencilImageView(uint32_t imageIndex) { return m_depthStencilBufferImages[imageIndex].imageView; };
  VkSemaphore& GetSemaphore(uint32_t imageIndex) { return m_renderAvailable[imageIndex]; };

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

  virtual void RecordCommands(uint32_t currentImage);
  void RecordLightingPassCommands(uint32_t currentImage);
  void RecordRaytracingShadowCommands(uint32_t currentImage);
  void RecordBoundingBoxCommands(uint32_t currentImage);
  void RecordObjectIDPassCommands(uint32_t currentImage);

 private:
  Editor* m_pEditor;

  // - Main Objects
  VkDevice m_pDevice;
  VkPhysicalDevice m_pPhyscialDevice;

  VkQueue m_pGraphicsQueue;

  uint32_t m_width;
  uint32_t m_height;
  Camera* m_pCamera;

  VkCommandPool m_pGraphicsCommandPool;
  std::vector<VkCommandBuffer> m_commandBuffers;

  std::vector<VkSemaphore> m_renderAvailable;
  std::vector<VkFence> m_fence;

  VkPushConstantRange m_debugPushConstant;

  // - Rendering Graphics Pipeline
  VkRenderPass m_renderPass;
  VkRenderPass m_objectIdRenderPass;
  VkRenderPass m_raytracingRenderPass;

  VkPipeline m_graphicsPipeline;
  VkPipeline m_wireGraphicsPipeline;  // Debugging (wireframe)
  VkPipeline m_boundingBoxPipeline;
  VkPipeline m_objectIDPipeline;
  VkPipelineLayout m_graphicsPipelineLayout;

  std::vector<GpuImage> m_colourBufferImages;
  std::vector<GpuImage> m_depthStencilBufferImages;

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

  std::vector<GpuImage> m_raytracingImages;

  ScratchBuffer m_scratchBufferTLAS;

  VkDescriptorPool m_raytracingPool;
  std::vector<VkDescriptorSet> m_raytracingSets;
  std::vector<VkDescriptorSetLayout> m_raytracingSetLayouts;

  std::vector<VkFramebuffer> m_framebuffers;
  VkFramebuffer m_objectIdFramebuffer;
  std::vector<VkFramebuffer> m_raytracingFramebuffers;
};
