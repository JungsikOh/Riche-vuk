//#pragma once
//
//#include "BatchSystem.h"
//#include "Components.h"
//#include "IRenderPass.h"
//#include "Utils/BoundingBox.h"
//#include "Utils/ModelLoader.h"
//#include "VkUtils/ChooseFunc.h"
//#include "VkUtils/DescriptorBuilder.h"
//#include "VkUtils/DescriptorManager.h"
//#include "VkUtils/QueueFamilyIndices.h"
//#include "VkUtils/ResourceManager.h"
//#include "VkUtils/ShaderModule.h"
//#include "VulkanRenderer.h"
//
//class Camera;
//
//
//class CullingRenderPass : public IRenderPass {
// public:
//  CullingRenderPass() = default;
//  CullingRenderPass(VkDevice device, VkPhysicalDevice physicalDevice);
//  ~CullingRenderPass() = default;
//
//  virtual void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, Camera* camera,
//                          Editor* editor, const uint32_t width, const uint32_t height);
//  virtual void Cleanup();
//
//  virtual void Update();
//
//  void SetupQueryPool();
//  void GetQueryResults();
//
//  virtual void Draw(uint32_t imageIndex, VkSemaphore renderAvailable);
//
//  VkImageView& GetFrameBufferImageView() { return m_depthOnlyBufferImage.imageView; };
//  VkSemaphore& GetSemaphore(uint32_t imageIndex) { return m_renderAvailable[imageIndex]; };
//
// private:
//  // - Rendering Pipeline
//  virtual void CreateRenderPass();
//
//  virtual void CreateFramebuffers();
//
//  virtual void CreatePipelineLayouts();
//  virtual void CreatePipelines();
//
//
//  virtual void CreateBuffers();
//
//
//  void CreatePushConstantRange();
//
//  void CreateSemaphores();
//  virtual void CreateCommandBuffers();
//  virtual void RecordCommands(uint32_t currentImage);
//
// private:
//  // - Main Objects
//  VkDevice m_pDevice;
//  VkPhysicalDevice m_pPhyscialDevice;
//
//  VkQueue m_pGraphicsQueue;
//
//  uint32_t m_width;
//  uint32_t m_height;
//  Camera* m_pCamera;
//
//  // - Rendering Graphics Pipeline
//  VkCommandPool m_pGraphicsCommandPool;
//  std::vector<VkCommandBuffer> m_commandBuffers;
//
//  std::vector<VkSemaphore> m_renderAvailable;
//  std::vector<VkFence> m_fence;
//
//  // -- Only Depth Rendering Pipeline
//  VkRenderPass m_depthRenderPass;
//
//  VkPipeline m_depthGraphicePipeline;  // Use a same GraphicsPipelineLayout
//  VkPipelineLayout m_graphicsPipelineLayout;
//
//  GpuImage m_depthOnlyBufferImage;
//  VkFramebuffer m_depthOnlyFramebuffer;  // mipmap 별로 생성.d
//};
