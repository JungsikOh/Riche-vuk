#pragma once

#include "BatchSystem.h"
#include "Components.h"
#include "CullingRenderPass.h"
#include "IRenderPass.h"
#include "VulkanRenderer.h"

class Camera;
class Editor;
class BasicLightingPass : public IRenderPass {
 public:
  BasicLightingPass() = default;
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

  virtual void CreateFramebuffer();
  void CreateObjectIdFramebuffer();

  virtual void CreatePipelineLayout();
  virtual void CreatePipeline();
  void CraeteGraphicsPipeline();
  void CreateWireGraphicsPipeline();
  void CreateBoundingBoxPipeline();
  void CreateObjectIDPipeline();

  virtual void CreateBuffers();
  void CreateBindlessResources();

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

  VkFramebuffer m_framebuffer;
  VkFramebuffer m_objectIdFramebuffer;

  VkCommandPool m_pGraphicsCommandPool;
  VkCommandBuffer m_commandBuffer;

  VkPushConstantRange m_debugPushConstant;

  VkSemaphore m_renderAvailable;
  VkFence m_fence;
};
