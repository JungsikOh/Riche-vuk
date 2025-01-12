#pragma once
#include "BatchSystem.h"
#include "Components.h"
#include "Editor/Editor.h"
#include "RenderSetting.h"
#include "Mesh.h"
#include "Swapchain.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/QueueFamilyIndices.h"
#include "VkUtils/ResourceManager.h"


static const int OBJECT_COUNT = 1000;


class Camera;
class Mesh;
class CullingRenderPass;

class VulkanRenderer {
 public:
  VulkanRenderer() = default;
  ~VulkanRenderer() = default;

  void Initialize(GLFWwindow* newWindow, Camera* newcamera);

  void Update();

  void Draw();
  void Cleanup();

 private:
  GLFWwindow* window;
  Camera* m_camera;
  int currentFrame = 0;
  bool enableValidationLayers;

  std::shared_ptr<Editor> m_pEditor;

  //
  // Vulkan Components
  // 
  // - Main
  VkInstance instance;
  struct {
    VkDevice logicalDevice;
    VkPhysicalDevice physicalDevice;
  } mainDevice;
  VkDebugUtilsMessengerEXT debugMessenger;

  VkQueue m_transferQueue;
  VkQueue m_computeQueue;
  VkQueue m_graphicsQueue;
  VkQueue m_presentationQueue;
  VkUtils::QueueFamilyIndices m_queueFamilyIndices;

  //
  // Swapcain features
  //
  VkSurfaceKHR m_swapchainSurface;
  VkSwapchainKHR m_swapchain;

  std::vector<SwapChainImage> m_swapchainImages;
  std::vector<SwapChainImage> m_swapchainDepthStencilImages;
  std::vector<VkDeviceMemory> m_swapchainDepthStencilImageMemories;
  std::vector<VkFramebuffer> m_swapchainFramebuffers;

  std::vector<VkCommandBuffer> m_swapchainCommandBuffers;

  // - Utility
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;

  // - Synchronisation
  std::vector<VkSemaphore> imageAvailable;
  std::vector<VkSemaphore> renderFinished;
  std::vector<VkFence> drawFences;

  //
  // OffScreen Features
  //
  VkRenderPass m_offScreenRenderPass;
  VkPipeline m_offScreenPipeline;
  VkPipelineLayout m_offScreenPipelineLayout;

  // - Rendering Pipelines
  std::shared_ptr<CullingRenderPass> m_pCullingRenderPass;

  entt::registry m_registry;

  // - Descriptors
  VkPushConstantRange pushConstantRange;

  // - Pools
  VkCommandPool m_graphicsCommandPool;
  VkCommandPool m_computeCommandPool;

  // - Samplers
  VkSampler m_linearWrapSS;
  VkSampler m_linearClampSS;
  VkSampler m_linearBorderSS;
  VkSampler m_pointWrapSS;
  VkSampler m_pointClampSS;

 private:
  // Vulkan Functions
  void CreateInstance();
  void CreateLogicalDevice();
  void SetupDebugMessnger();
  
  //
  // For Swapchains
  //
  void CreateSurface();
  void CreateSwapChain();
  void CreateSwapchainFrameBuffers();
  //
  // Rendering Pipeline
  //
  void CreateRenderPass();
  void CreateOffScreenRenderPass();
  void CreateOffScrrenDescriptorSet();
  void CreatePipeline();

  void CreatePushConstantRange();

  //
  // Command Queue
  //
  void CreateCommandPool();
  void CreateCommandBuffers();
  void CreateSynchronisation();

  // - Record Functions
  void RecordCommands(uint32_t currentImage);
  void FillOffScreenCommands(uint32_t currentImage);

  // - Get Functions
  void GetPhysicalDevice();

  // - Allocate Functions
  void AllocateDynamicBufferTransferSpace();

  // - Set Functions
  void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  // - Support Functions
  // -- Checker Functions
  bool CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
  bool CheckValidationLayerSupport(const std::vector<const char*>* checkVaildationLayers);
  bool CheckDeviceSuitable(VkPhysicalDevice device);

  // -- Getter Functions
  SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device);

  // -- Choose Functions (For Swapchain)
  VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
  VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
};
