#pragma once

#include "VkUtils/DescriptorManager.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/QueueFamilyIndices.h"

class Editor
{
	GLFWwindow* m_Window;
	VkInstance m_Instance;
	struct {
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;
	} mainDevice;

	VkUtils::QueueFamilyIndices m_QueueFamilyIndices;
	VkQueue m_GraphicsQueue;

	VkDescriptorPool m_ImguiDescriptorPool;

public:
	void Initialize(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue);
	void CreateImGuiDescriptorPool();
};

