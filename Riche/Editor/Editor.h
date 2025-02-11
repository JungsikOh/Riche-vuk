#pragma once

#include "Rendering/Components.h"
#include "Rendering/RenderSetting.h"
#include "Rendering/CullingRenderPass.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/QueueFamilyIndices.h"
#include "VkUtils/ResourceManager.h"

class Camera;

class Editor {
  GLFWwindow* m_Window;
  Camera* m_pCamera;
  int m_selectedIndex = -1;
  int m_gizmoType = -1;

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
                  VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue, Camera* camera);
  void Cleanup();

  void Update();

  void RenderImGui(VkCommandBuffer commandBuffer);

  void OnLeftMouseClick();

 private:
  void CreateImGuiDescriptorPool();

  void UpdateKeyboard();
};
