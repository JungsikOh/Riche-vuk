#pragma once
#include <filesystem>

#include "Utils/ModelLoader.h"
#include "VkUtils/ChooseFunc.h"
#include "VkUtils/QueueFamilyIndices.h"
#include "VkUtils/ResourceManager.h"

class Camera;
class BasicLightingPass;

class Editor {
  bool check = false;
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
  CullingRenderPass* m_pCullingPass;
  BasicLightingPass* m_pLightingPass;

 public:
  Editor() = default;
  ~Editor() = default;

  void Initialize(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
                  VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue, Camera* camera);

  void Cleanup();

  void Update();

  void RenderImGui(VkCommandBuffer commandBuffer);

  void OnLeftMouseClick();

 private:
  void CreateImGuiDescriptorPool();
  void ShowFileBrowserUI(const std::string& filter);
  void UpdateKeyboard();
};
