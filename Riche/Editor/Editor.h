#pragma once
#include "Rendering/RenderSetting.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/QueueFamilyIndices.h"
#include "VkUtils/ResourceManager.h"

class Editor {
  GLFWwindow* m_Window;
  VkInstance m_Instance;
  struct {
    VkDevice logicalDevice;
    VkPhysicalDevice physicalDevice;
  } mainDevice;

  VkUtils::QueueFamilyIndices m_queueFamilyIndices;
  VkCommandPool m_GraphicsPool;
  VkQueue m_graphicsQueue;

  VkDescriptorPool m_ImguiDescriptorPool;
  VkRenderPass renderPass;

 public:
  void Initialize(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
                  VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue);
  void Cleanup();

  void RenderImGui(VkCommandBuffer commandBuffer);

 private:
  void CreateImGuiDescriptorPool();
};
