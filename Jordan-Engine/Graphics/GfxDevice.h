#pragma once

#include <iostream>
#include <array>
#include <vector>
#include <optional>
#include <mutex>
#include <string>
#include <set>
#include <memory>
#include <assert.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "GfxCore.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily = std::nullopt;
	VkQueueFamilyProperties graphicsQueueProperties;
	std::optional<uint32_t> presentationFamily = std::nullopt;
	VkQueueFamilyProperties presentationQueueProperties;
	std::optional<uint32_t> computeFamily = std::nullopt;
	VkQueueFamilyProperties computeQueueProperties;
	std::optional<uint32_t> transferFamily = std::nullopt;
	VkQueueFamilyProperties transferQueueProperties;

	bool isVaild()
	{
		return graphicsFamily.has_value() && presentationFamily.has_value() && computeFamily.has_value() && transferFamily.has_value();
	}
};

struct SwapchainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities;			// surface properites, e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;				// surface image foramts, e.g. RGBA and size of each color
	std::vector<VkPresentModeKHR> presentationModes;		// how images should be presented to screen, e.g. IMMEDAITE and MAILBOX etc..
};

struct GfxSwapchain
{
	VkSwapchainKHR swapchain;
	VkSurfaceKHR surface;

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;

	VkFormat format;
	VkExtent2D extent = { 0, 0 };
};

class GfxQueue;

class GfxDevice
{
public:
	GfxDevice() = default;
	~GfxDevice() = default;

	void Initialize(GLFWwindow* newWindow);
	void Destroy();

	GLFWwindow* window = nullptr;

	/// Vulkan Components
	// - Main
	VkInstance instance;
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;

	VkDebugUtilsMessengerEXT debugMessenger;

	std::unique_ptr<GfxQueue> graphicsQueue = nullptr;
	std::unique_ptr<GfxQueue> presentationQueue = nullptr;
	std::unique_ptr<GfxQueue> computeQueue = nullptr;
	std::unique_ptr<GfxQueue> transferQueue = nullptr;

	GfxSwapchain swapchain {};

	// - Device Properties
	VkPhysicalDeviceProperties physicalDeviceProperties;
	size_t minBufferAlignment;

private:
	// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	void GetPhysicalDevice();
	void CreateSurface();
	void CreateSwapchain();

	// - Support Functions
	// -- Getter Functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	SwapchainDetails GetSwapchainDetails(VkPhysicalDevice device);

	// -- Checker Functions
	bool CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckValidationLayerSupport(const std::vector<const char*>* checkVaildationLayers);
	bool CheckDeviceSuitable(VkPhysicalDevice device);

	// -- Debug Functions
	bool EnableDebug();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		std::cerr << "Validation layer: " << pCallbackData->pMessage << "\n" << std::endl;

		return VK_FALSE;
	};
};

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}