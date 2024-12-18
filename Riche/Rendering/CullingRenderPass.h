#pragma once

#include "BatchSystem.h"
#include "Components.h"
#include "IRenderPass.h"

class Camera;

class CullingRenderPass : public IRenderPass {
 public:
  CullingRenderPass() = default;
  ~CullingRenderPass() = default;

  virtual void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, Camera* camera,
                          const uint32_t width, const uint32_t height);
  virtual void Cleanup();

  virtual void Update();

  virtual void Draw(VkSemaphore renderAvailable);

  VkImageView& GetFrameBufferImageView() { return m_colourBufferImageView; };
  VkImageView& GetDepthStencilImageView() { return m_depthStencilBufferImageView; };
  VkSemaphore& GetSemaphore() { return m_renderAvailable; };

 private:
  // - Rendering Pipeline
  virtual void CreateRenderPass();
  void CreateDepthRenderPass();

  virtual void CreateFramebuffer();
  void CreateDepthFramebuffer();

  virtual void CreatePipeline();
  void CraeteGrahpicsPipeline();
  void CreateWireGraphicsPipeline();
  void CreateDepthGraphicsPipeline();
  void CraeteViewCullingComputePipeline();
  void CreateOcclusionCullingComputePipeline();

  virtual void CreateBuffers();
  void CreateShaderStorageBuffers();
  void CreateUniformBuffers();
  void CreateDesrciptorSets();

  void CreateSemaphores();
  virtual void CreateCommandBuffers();
  virtual void RecordCommands();

 private:
  // - Main Objects
  VkDevice m_pDevice;
  VkPhysicalDevice m_pPhyscialDevice;

  VkQueue m_pGraphicsQueue;

  uint32_t m_width;
  uint32_t m_height;
  Camera* m_pCamera;

  // - Rendering Graphics Pipeline
  VkRenderPass m_renderPass;

  VkPipeline m_graphicsPipeline;
  VkPipelineLayout m_graphicsPipelineLayout;

  // -- Debugging (wireframe)
  VkPipeline m_wireGraphicsPipeline;

  VkImage m_colourBufferImage;
  VkDeviceMemory m_colourBufferImageMemory;
  VkImageView m_colourBufferImageView;

  VkImage m_depthStencilBufferImage;
  VkDeviceMemory m_depthStencilBufferImageMemory;
  VkImageView m_depthStencilBufferImageView;

  VkFramebuffer m_framebuffer;

  VkCommandPool m_pGraphicsCommandPool;
  VkCommandBuffer m_commandBuffer;

  VkSemaphore m_renderAvailable;
  VkFence m_fence;

  // -- Only Depth Rendering Pipeline
  VkRenderPass m_depthRenderPass;

  VkPipeline m_depthGraphicePipeline;   // Use a same GraphicsPipelineLayout

  VkImage m_onlyDepthBufferImage;
  VkDeviceMemory m_onlyDepthBufferImageMemory;
  VkImageView m_onlyDepthBufferImageView;

  VkFramebuffer m_depthFramebuffer;

  // -- Compute Pipeline
  VkPipeline m_viewCullingComputePipeline;
  VkPipelineLayout m_viewCullingComputePipelineLayout;

  VkPipeline m_occlusionCullingComputePipeline;

  // -- Indirect Draw Call
  VkBuffer m_indirectDrawBuffer;
  VkDeviceMemory m_indirectDrawBufferMemory;

  // -- Descriptor Set
  ViewProjection m_viewProjectionCPU;
  VkBuffer m_viewProjectionUBO;
  VkDeviceMemory m_viewProjectionUBOMemory;

  std::vector<glm::mat4> m_modelListCPU;
  VkBuffer m_modelListUBO;
  VkDeviceMemory m_modelListUBOMemory;

  std::vector<AABB> m_aabbList;
  VkBuffer m_aabbListBuffer;
  VkDeviceMemory m_aabbListBufferMemory;

  VkBuffer m_cameraFrustumBuffer;
  VkDeviceMemory m_cameraFrustumBufferMemory;

  // - Rendering Object
  std::vector<MiniBatch> m_miniBatchList;
};
