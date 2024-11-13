#pragma once
#include "Camera.h"
#include "Components.h"
#include "Swapchain.h"
#include "Mesh.h"

#include "Editor/Editor.h"

#include "VkUtils/DescriptorManager.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/QueueFamilyIndices.h"

static const int OBJECT_COUNT = 10;

class Mesh;

class DescriptorManager;
class DescriptorBuilder;

class VulkanRenderer
{
public:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;

	int Initialize(GLFWwindow* newWindow, Camera* newCamera);

	void UpdateModel(int modelId, glm::mat4 newModel);
	void UpdateKeyInput();

	void Draw();
	void Cleanup();

private:
	GLFWwindow* window;
	Camera* camera;
	int currentFrame = 0;
	bool enableValidationLayers;

	Editor editor;

	// Scene Objects
	std::vector<Mesh> meshes;

	// Scene Settings
	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;

	Model model;

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
	VkUtils::ResourceManager m_ResoucreManager;


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
	VkPipeline m_OffScreenPipeline;
	VkPipelineLayout m_OffScreenPipelineLayout;
	std::vector<VkCommandBuffer> m_SwapchainCommandBuffers;

	//
	// Basic Render Features
	//
	VkRenderPass m_BasicRenderPass;
	VkPipeline m_BasicPipeline;
	VkPipelineLayout m_BasicPipelineLayout;
	VkCommandBuffer m_BasicCommandBuffer;

	VkImage m_ColourBufferImage;
	VkDeviceMemory m_ColourBufferImageMemory;
	VkImageView m_ColourBufferImageView;

	VkImage m_DepthBufferImage;
	VkDeviceMemory m_DepthBufferImageMemory;
	VkImageView m_DepthBufferImageView;

	VkFramebuffer m_BasicFramebuffer;

	ViewProjection m_ViewProjectionCPU;
	VkBuffer m_ViewProjectionUBO;
	VkDeviceMemory m_ViewProjectionUBOMemory;

	// -- For Compute shader
	VkPipeline m_ViewCullingComputePipeline;
	VkPipelineLayout m_ViewCullingComputePipelineLayout;

	VkBuffer m_InDirectDrawBuffer;
	VkDeviceMemory m_InDirectDrawBufferMemory;
	VkBuffer m_BoundingBoxes;
	VkDeviceMemory m_BoundingBoxesMemory;
	VkBuffer m_CameraBoudingBox;
	VkDeviceMemory m_CameraBoudingBoxMemory;

	VkSemaphore m_BasicRenderAvailable;
	VkFence m_BasicFence;

	entt::registry m_Registry;

	// - Descriptors
	VkPushConstantRange pushConstantRange;

	//VkDeviceSize minUniformBufferOffset;
	//size_t modelUniformAligment;
	//Model* modelTransferSpace;

	// - Pools
	VkCommandPool m_GraphicsCommandPool;
	VkCommandPool m_ComputeCommandPool;

	// - Utility
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	// - Synchronisation
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

	VkSampler textureSampler;

private:
	// Vulkan Functions
	// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	void SetupDebugMessnger();
	void CreateRenderPass();
	//
	// For Swapchains
	//
	void CreateSurface();
	void CreateSwapChain();
	void CreateSwapchainFrameBuffers();
	void CreateOffScreenRenderPass();
	void CreateOffScrrenDescriptorSet();
	void CreateOffScreenPipeline();

	void CreateBasicFramebuffer();
	void CreateBasicRenderPass();
	void CreateBasicPipeline();
	void CreateBasicSemaphores();

	void CreateInDirectDrawBuffer();
	void CreateComputePipeline();

	void CreateDescriptorSetLayout();
	void CreatePushConstantRange();
	
	void CreateCommandPool();
	void CraeteSwapchainCommandPool();

	void CreateCommandBuffers();
	void CreateSynchronisation();

	void CreateUniformBuffers();
	void CreateDescriptorSets();

	void UpdateUniformBuffers(uint32_t imageIndex);

	// - Record Functions
	void RecordCommands(uint32_t currentImage);
	void FillOffScreenCommands(uint32_t currentImage);
	void FillBasicCommands();

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

};

