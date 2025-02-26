#pragma once

#include "BatchSystem.h"
#include "Components.h"
#include "Editor/Editor.h"
#include "IRenderPass.h"
#include "Mesh.h"
#include "Utils/BoundingBox.h"
#include "Utils/ModelLoader.h"
#include "VkUtils/ChooseFunc.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/QueueFamilyIndices.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/ShaderModule.h"
#include "VulkanRenderer.h"

const static int HIZ_MIP_LEVEL = 3;

class Camera;
class CullingRenderPass : public IRenderPass {
 public:
  CullingRenderPass() = default;
  CullingRenderPass(VkDevice device, VkPhysicalDevice physicalDevice);
  ~CullingRenderPass() = default;

  virtual void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, Camera* camera,
                          Editor* editor, const uint32_t width, const uint32_t height);
  virtual void Cleanup();

  virtual void Update();

  void SetupQueryPool();
  void GetQueryResults();

  virtual void Draw(uint32_t imageIndex, VkSemaphore renderAvailable);

  VkImageView& GetFrameBufferImageView() { return m_depthOnlyBufferImage.imageView; };
  VkSemaphore& GetSemaphore(uint32_t imageIndex) { return m_renderAvailable[imageIndex]; };

 private:
  // - Rendering Pipeline
  virtual void CreateRenderPass();
  void CreateDepthRenderPass();

  virtual void CreateFramebuffers();
  void CreateDepthFramebuffer();

  virtual void CreatePipelineLayouts();
  virtual void CreatePipelines();
  void CreateDepthGraphicsPipeline();
  void CraeteViewCullingComputePipeline();
  void CreateOcclusionCullingComputePipeline();

  virtual void CreateBuffers();
  void CreateShaderStorageBuffers();
  void CreateUniformBuffers();
  void CreateDesrciptorSets();

  void CreatePushConstantRange();

  void CreateSemaphores();
  virtual void CreateCommandBuffers();
  virtual void RecordCommands(uint32_t currentImage);
  void RecordViewCullingCommands(uint32_t currentImage);
  void RecordOnlyDepthCommands(uint32_t currentImage);
  void RecordOcclusionCullingCommands(uint32_t currentImage);

 private:
  // - Main Objects
  VkDevice m_pDevice;
  VkPhysicalDevice m_pPhyscialDevice;

  VkQueue m_pGraphicsQueue;

  uint32_t m_width;
  uint32_t m_height;
  Camera* m_pCamera;

  // - Rendering Graphics Pipeline
  VkCommandPool m_pGraphicsCommandPool;
  std::vector<VkCommandBuffer> m_commandBuffers;

  std::vector<VkSemaphore> m_renderAvailable;
  std::vector<VkFence> m_fence;

  // -- Only Depth Rendering Pipeline
  VkRenderPass m_depthRenderPass;

  VkPipeline m_depthGraphicePipeline;  // Use a same GraphicsPipelineLayout
  VkPipelineLayout m_graphicsPipelineLayout;

  GpuImage m_depthOnlyBufferImage;
  VkFramebuffer m_depthOnlyFramebuffer;  // mipmap 별로 생성.

  VkQueryPool m_occlusionQueryPool;
  std::vector<uint64_t> m_passedSamples;

  // -- Compute Pipeline
  VkPipeline m_viewCullingComputePipeline;
  VkPipelineLayout m_viewCullingComputePipelineLayout;

  VkPipeline m_occlusionCullingComputePipeline;
  VkPipelineLayout m_occlusionCullingComputePipelineLayout;

  // -- For debuging
  VkBuffer m_fLODListBuffer;
  VkDeviceMemory m_fLODListBufferMemory;

  VkPushConstantRange m_debugPushConstant;

  std::array<FrustumPlane, 6> m_frustumPlanes;
  VkBuffer m_cameraFrustumBuffer;
  VkDeviceMemory m_cameraFrustumBufferMemory;
};
