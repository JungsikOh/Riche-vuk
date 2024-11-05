#pragma once
<<<<<<< HEAD

#include "IRenderer.h"
#include "Components.h"
#include "Graphics/GfxCore.h"
#include "DescriptorManager.h"

class DescriptorManager;

class GfxDevice;
class GfxPipeline;
class GfxFrameBuffer;
class GfxRenderPass;
class GfxSubpass;
class GfxResourceManager;
class GfxImage;

class VulkanRenderer : public IRenderer
{
	static const UINT MAX_DRAW_COUNT_PER_FRAME = 4096;
	static const UINT MAX_DESCRIPTOR_COUNT = 4096;
	static const UINT MAX_RENDER_THREAD_COUNT = 8;
	
	// Gfx
	std::shared_ptr<GfxDevice> m_pDevice;

	std::shared_ptr<GfxCommandPool> m_pGraphicsCommandPool;
	std::vector<std::shared_ptr<GfxCommandBuffer>> m_pCommandBuffers;

	// Swapchain
	std::vector<std::shared_ptr<GfxImage>> m_pSwapchainDepthStencilImages;
	std::vector<std::shared_ptr<GfxFrameBuffer>> m_pSwapchainFrameBuffers;

	std::shared_ptr<GfxPipeline> m_pGraphicsPipeline;

	std::vector<GfxSubpass> m_subpasses;
	std::shared_ptr<GfxRenderPass> m_pRenderPass;

	std::shared_ptr<GfxResourceManager> m_pResourceManager;
	
	// Descriptor
	std::shared_ptr<GfxDescriptorLayoutCache> m_pDescriptorLayoutCache;
	std::shared_ptr<GfxDescriptorAllocator> m_pDescriptorAllocator;

	std::vector<GfxDescriptorBuilder> m_inputDescriptorBuilder;
	GfxDescriptorBuilder m_uboDescriptorBuilder;

	DescriptorManager m_descriptorManager;

	Mesh firstMesh = {};
	std::shared_ptr<GfxBuffer> m_pUboViewProjBuffer;

=======
#include "Camera.h"
#include "Components.h"
#include "Swapchain.h"
#include "Mesh.h"

#include "VkUtils/DescriptorManager.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/QueueFamilyIndices.h"

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

	// Scene Objects
	Mesh mesh;

	// Scene Settings
>>>>>>> origin/second
	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;

<<<<<<< HEAD

public:
	VulkanRenderer();
	~VulkanRenderer();

	// Derived from IRenderer
	virtual BOOL	__stdcall Initialize(GLFWwindow* window);
	virtual void	__stdcall BeginRender();
	//virtual void	__stdcall EndRender() = 0;
	//virtual void	__stdcall Present() = 0;
	///*virtual BOOL	__stdcall UpdateWindowSize(DWORD dwBackBufferWidth, DWORD dwBackBufferHeight) = 0;*/

	void FillCommandBuffer();

	//virtual void* __stdcall CreateTiledTexture(UINT TexWidth, UINT TexHeight, DWORD r, DWORD g, DWORD b) = 0;
	//virtual void* __stdcall CreateDynamicTexture(UINT TexWidth, UINT TexHeight) = 0;
	//virtual void* __stdcall CreateTextureFromFile(const WCHAR* wchFileName) = 0;
	//virtual void	__stdcall DeleteTexture(void* pTexHandle) = 0;

	//virtual void	__stdcall UpdateTextureWithImage(void* pTexHandle, const BYTE* pSrcBits, UINT SrcWidth, UINT SrcHeight) = 0;

	//virtual void	__stdcall SetCameraPos(float x, float y, float z) = 0;
	//virtual void	__stdcall MoveCamera(float x, float y, float z) = 0;
	//virtual void	__stdcall GetCameraPos(float* pfOutX, float* pfOutY, float* pfOutZ) = 0;
	//virtual void	__stdcall SetCamera(const glm::vec4 pCamPos, const glm::vec4 pCamDir, const glm::vec4 pCamUp) = 0;
	//virtual DWORD	__stdcall GetCommandListCount() = 0;

private:
	void RenderBasicObject();

	void CreateSubpass();
	void CreateRenderPass();
	void CreateFrameBuffer();
	void CreateGraphicsPipeline();
};

static std::wstring ToWideString(std::string const& in) {
	std::wstring out{};
	out.reserve(in.length());
	const char* ptr = in.data();
	const char* const end = in.data() + in.length();

	mbstate_t state{};
	wchar_t wc;
	while (size_t len = mbrtowc(&wc, ptr, end - ptr, &state)) {
		if (len == static_cast<size_t>(-1)) // bad encoding
			return std::wstring{};
		if (len == static_cast<size_t>(-2))
			break;
		out.push_back(wc);
		ptr += len;
	}
	return out;
}
=======
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

	VkSemaphore m_BasicRenderAvailable;
	VkFence m_BasicFence;

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

>>>>>>> origin/second
