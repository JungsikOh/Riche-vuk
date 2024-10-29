#pragma once

#include <vulkan/vulkan.hpp>

#include "IRenderer.h"
#include "Components.h"
#include "Swapchain.h"

#include "VkUtils/DescriptorManager.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/QueueFamilyIndices.h"

class DescriptorManager;
class DescriptorBuilder;

class VulkanRenderer
{
public:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;

	int Initialize(GLFWwindow* newWindow);

	void UpdateModel(int modelId, glm::mat4 newModel);
	void CreateMeshModel(std::string modelFile);

	void Draw();
	void Cleanup();

private:
	GLFWwindow* window;
	int currentFrame = 0;
	bool enableValidationLayers;

	// Scene Objects
	std::vector<Mesh> meshes;

	// Scene Settings
	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;

	// Vulkan Components
	// - Main
	VkInstance instance;
	struct {
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;
	} mainDevice;
	VkDebugUtilsMessengerEXT debugMessenger;

	VkQueue m_TransferQueue;
	VkQueue m_ComputeQueue;
	VkQueue m_GraphicsQueue;
	VkQueue m_PresentationQueue;
	VkUtils::QueueFamilyIndices m_QueueFamilyIndices;

	std::shared_ptr<VkUtils::DescriptorAllocator> m_pDescriptorAllocator = nullptr;
	std::shared_ptr<VkUtils::DescriptorLayoutCache> m_pLayoutCache = nullptr;

	VkUtils::DescriptorManager m_DescriptorManager;


	//
	// Swapcain features
	//
	VkSurfaceKHR m_SwapchainSurface;
	VkSwapchainKHR m_Swapchain;

	std::vector<SwapChainImage> m_SwapChainImages;
	std::vector<SwapChainImage> m_SwapchainDepthStencilImages;
	std::vector<VkDeviceMemory> m_SwapchainDepthStencilImageMemories;
	std::vector<VkFramebuffer> m_SwapChainFramebuffers;

	std::vector<VkDescriptorSet> m_OffScreenDescriptorSets;
	VkDescriptorSetLayout m_OffScreenSetLayout;

	VkRenderPass m_OffScreenRenderPass;
	VkRenderPass m_BasicRenderPass;

	std::vector<VkCommandBuffer> commandBuffers;

	VkImage m_ColourBufferImage;
	VkDeviceMemory m_ColourBufferImageMemory;
	VkImageView m_ColourBufferImageView;

	VkImage m_DepthBufferImage;
	VkDeviceMemory m_DepthBufferImageMemory;
	VkImageView m_DepthBufferImageView;

	//std::vector<VkImage> depthBufferImage;
	//std::vector<VkDeviceMemory> depthBufferImageMemory;
	//std::vector<VkImageView>depthBufferImageView;

	VkSampler textureSampler;

	// - Descriptors
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout samplerSetLayout;
	VkDescriptorSetLayout inputDescriptorSetLayout;
	VkPushConstantRange pushConstantRange;

	VkDescriptorPool descriptorPool;
	VkDescriptorPool samplerDescriptorPool;
	VkDescriptorPool inputDescriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkDescriptorSet> samplerDescriptorSets;
	std::vector<VkDescriptorSet> inputDescriptorSets;

	std::vector<VkBuffer> vpUniformBuffer;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;

	std::vector<VkBuffer> modelDUniformBuffer;
	std::vector<VkDeviceMemory> modelDUniformBufferMemory;


	//VkDeviceSize minUniformBufferOffset;
	//size_t modelUniformAligment;
	//Model* modelTransferSpace;

	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory;
	std::vector<VkImageView> textureImageViews;

	// - Pipeline
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;

	VkPipeline secondPipeline;
	VkPipelineLayout secondPipelineLayout;

	VkRenderPass renderPass;

	// - Pools
	VkCommandPool graphicsCommandPool;

	// - Utility
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	// - Synchronisation
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

private:
	// Vulkan Functions
	// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	void SetupDebugMessnger();
	//
	// For Swapchains
	//
	void CreateSurface();
	void CreateSwapChain();
	void CreateRenderPass();
	void CreateBasicRenderPass();
	void CreateOffScreenRenderPass();
	void CreateOffScrrenDescriptorSet();
	void CreateDescriptorSetLayout();
	void CreatePushConstantRange();
	void CreateOffScreenPipeline();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSynchronisation();
	void CreateTextureSampler();

	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreateInputDescriptorSets();

	void UpdateUniformBuffers(uint32_t imageIndex);

	// - Record Functions
	void RecordCommands(uint32_t currentImage);

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

	// -- Choose Functions
	VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkFormat ChooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

	// -- Create Functions
	VkImage CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags,
		VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory);

	int CreateTextureImage(std::string filename);
	int CreateTexture(std::string filename);
	int CreateTextureDescriptor(VkImageView textureImage);

	// -- Loader Functions
	stbi_uc* LoadTextureFile(std::string filename, int* width, int* height, VkDeviceSize* imageSize);
};

