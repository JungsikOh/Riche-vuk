#include "VulkanRenderer.h"
#include "Core.h"

#include "Utils/StringUtil.h"

#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/ShaderModule.h"

namespace
{
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}
};

int VulkanRenderer::Initialize(GLFWwindow* newWindow)
{
	window = newWindow;

	try {
		CreateInstance();
		SetupDebugMessnger();
		GetPhysicalDevice();
		CreateSurface();
		CreateLogicalDevice();
		CreateSwapChain();

		CreateRenderPass();

		m_pDescriptorAllocator = std::make_shared<VkUtils::DescriptorAllocator>();
		m_pDescriptorAllocator->Initialize(mainDevice.logicalDevice);
		m_pLayoutCache = std::make_shared<VkUtils::DescriptorLayoutCache>();
		m_pLayoutCache->Initialize(mainDevice.logicalDevice);

		CreateOffScrrenDescriptorSet();

		CreateOffScreenPipeline();

		CreateDescriptorSetLayout();
		CreatePushConstantRange();
		CreateCommandPool();
		CreateCommandBuffers();
		CreateTextureSampler();
		//AllocateDynamicBufferTransferSpace();
		CreateUniformBuffers();
		CreateDescriptorPool();
		CreateDescriptorSets();
		CreateInputDescriptorSets();
		CreateSynchronisation();

		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(2.0f, 0.0f, 100.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		uboViewProjection.projection[1][1] *= -1;	// opengl is Y-Up but, vulkan is Y-Down. �ٸ�, ��������� Y-Down�� ���̴�.

		// Create a mesh
		// Vulkan�� viewport��ǥ��� projection ����� Y-Down
		// Clip Space�� NDC ������ �⺻������ Y-Down�̴�.
		std::vector<BasicVertex> meshVertices = {
			{ { 1.0, -1.7, 0.0 },{ 1.0f, 0.0f, 0.0f }, {1.0f, 1.0f} },	// 0
			{ { 1.0, 0.4, 0.0 },{ 0.0f, 1.0f, 0.0f }, {1.0f, 0.0f} },	    // 1
			{ { -0.4, 0.4, 0.0 },{ 0.0f, 0.0f, 1.0f }, {0.0f, 0.0f} },    // 2
			{ { -0.4, -0.4, 0.0 },{ 1.0f, 1.0f, 0.0f }, {0.0f, 1.0f} },   // 3
		};

		std::vector<BasicVertex> meshVertices2 = {
			{ { 1.0, -1.0, 0.0 },{ 1.0f, 0.0f, 0.0f }, {1.0f, 1.0f} },	// 0
			{ { 1.0, 0.4, 0.0 },{ 1.0f, 0.0f, 0.0f }, {1.0f, 0.0f} },	    // 1
			{ { -0.4, 0.4, 0.0 },{ 1.0f, 0.0f, 1.0f }, {0.0f, 0.0f} },    // 2
			{ { -0.4, -0.4, 0.0 },{ 1.0f, 0.0f, 0.0f }, {0.0f, 1.0f} },   // 3
		};

		// Index Data
		std::vector<uint32_t> meshIndices = {
			0, 1, 2,
			2, 3, 0
		};

		Mesh firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, m_GraphicsQueue, graphicsCommandPool, &meshVertices, &meshIndices, CreateTexture("screenshot.png"));
		Mesh secondMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, m_GraphicsQueue, graphicsCommandPool, &meshVertices2, &meshIndices, CreateTexture("screenshot.png"));
		meshes.push_back(firstMesh);
		meshes.push_back(secondMesh);
	}
	catch (const std::runtime_error& e) {
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::UpdateModel(int modelId, glm::mat4 newModel)
{
	if (modelId > modelList.size()) return;

	modelList[modelId].SetModel(newModel);
}

void VulkanRenderer::Draw()
{
	// 1. Get next available image to draw to and set something to signal when we're finished with the image (a semaphore)

	// Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint32_t>::max());
	// Manually reset (close) fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	// -- Get Next Image --, Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;	// swapchain�� �̹��� ���ۿ��� index ���� ����´�.
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint32_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	RecordCommands(imageIndex);
	UpdateUniformBuffers(imageIndex);

	// 2. Submit command buffer to queue for execution, making sure it waits for the image to be signalled as available before drawing
	// and signals when it has finished rendering (wait a fence)
	// -- SUBMIT COOMAND BUUFER TO RENDER --, Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;									// Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];			// List of semaphores to wait on, Command buffer�� �����ϱ��� ����ؾ��ϴ� semaphores
	// ��, �� semphore�� signaled ���°� �� ������ ����ϰ�, �� �Ŀ� Command buffer�� �����Ѵ�.
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;									// Stages to check semaphores at
	submitInfo.commandBufferCount = 1;											// Number of command buffers to submit
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];					// Command buffer to submit
	submitInfo.signalSemaphoreCount = 1;										// Number of semaphores to signal
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];				// Semaphores to signal when command buffer finishes
	// Command buffer�� ������ �Ϸ��ϸ�, Signaled ���°� �� semaphore �迭.

// Submit command buffer to queue
	VkResult result = vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue");
	}

	// 3. Present image to screen when it has signalled finished rendering (then reset a fence)
	// -- PRESENT RENDERED IMAGE TO SCREEN --
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;									// Number of semaphores to wait on
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];		// Semaphores to wait on
	presentInfo.swapchainCount = 1;										// Number of swapchains to present to
	presentInfo.pSwapchains = &swapchain;								// swapchain to present images to
	presentInfo.pImageIndices = &imageIndex;							// Index of images in swapchain to present

	result = vkQueuePresentKHR(m_PresentationQueue, &presentInfo);		// ������ �Ϸ�� Graphicsť�� present �ϴ� �Լ�. ��, presentQueue�� �����Ѵٰ� �� �� ����.
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to Present swapchain!");
	}

	// Get next frame
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::Cleanup()
{
	// Wait Until no actions being run on device before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputDescriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	for (size_t i = 0; i < textureImages.size(); ++i)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < m_ColourBufferImage.size(); ++i)
	{
		vkDestroyImageView(mainDevice.logicalDevice, m_ColourBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, m_ColourBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, m_ColourBufferImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < depthBufferImage.size(); ++i)
	{
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
	//for (size_t i = 0; i < modelDUniformBuffer.size(); ++i)
	//{
	//	vkDestroyBuffer(mainDevice.logicalDevice, modelDUniformBuffer[i], nullptr);
	//	vkFreeMemory(mainDevice.logicalDevice, modelDUniformBufferMemory[i], nullptr);
	//}
	for (size_t i = 0; i < vpUniformBuffer.size(); ++i)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
	}

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	//vkFreeCommandBuffers(mainDevice.logicalDevice, graphicsCommandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);

	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (auto image : m_SwapChainImages)
	{
		// VkImageView�� VkImage�� ���� �����ؼ� �並 ������ ���̹Ƿ�, ȣ���ؼ� �ı��ؾ��Ѵ�.
		// �ݸ�, VkImage�� ����ü���� ������ �� �Ҵ�� �޸� ������ �����ǹǷ� �ı��� �ʿ䰡 ����.
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, m_SwapchainSurface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::CreateInstance()
{
	// Information about the application itself
	// Most data here doens't affect the program and is for developer convenience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";				// Custom name of the application
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);	// Custion verstion of the application
	appInfo.pEngineName = "No Engine";						// Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);		// Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_3;				// The Vulkan Version

	// Creation Information for a VkInstance (Vulkan Instance)
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// layers need to be enabled by specifying their name. This name is standard validation.
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

#ifdef NDEBUG	// NDEBUG is C++ standard Macro.
	enableValidationLayers = false;
#else
	enableValidationLayers = true;
#endif

	if (enableValidationLayers && !CheckValidationLayerSupport(&validationLayers))
	{
		throw std::runtime_error("Valdation Layers requested, but not available");
	}

	// Setup debugger
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();

	// Set up extensions Instance will use
	uint32_t glfwExtensionCount = 0;			// GLFW may require multiple extensions
	const char** glfwExtensions;				// Extensions passed as array of cstrings, so need pointer(the array) to pointer (the cstring)

	// get GLFW extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Add GLFW extensions to list of extenesions
	for (size_t i = 0; i < glfwExtensionCount; ++i)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	// Check Instance Extensions supported...
	if (!CheckInstanceExtensionSupport(&instanceExtensions))
	{
		throw std::runtime_error("VkInstance does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// Create Instance
	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
}

void VulkanRenderer::CreateLogicalDevice()
{
	// Get the Queue family indices for the chosen physical device
	m_QueueFamilyIndices = VkUtils::GetQueueFamilies(mainDevice.physicalDevice, m_SwapchainSurface);

	// Vector for queue creation information, and set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { m_QueueFamilyIndices.transferFamily, m_QueueFamilyIndices.computeFamily,
		m_QueueFamilyIndices.graphicsFamily, m_QueueFamilyIndices.presentationFamily };

	// Queues the logical device needs to create and info to do so
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;					// The Index of the family to create a queue from
		queueCreateInfo.queueCount = 1;											// Number of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;							// Vulkan needs to know thow to handle multiple queues, so decide priority ( 1 = highest priority)

		queueCreateInfos.push_back(queueCreateInfo);
	}

	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	// Information to create logical device (someties called device)
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());				// Number of queue create Infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();										// List of Queue Create Infos so device can craete required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());			// Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();									// List of enabled logical device extentions

	// Pyhsical Device Features the logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.depthBiasClamp = VK_FALSE;								// Rasterizer���� �ش� ����� ������ �������� ���� ����
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;					// Physical device features logical device will use

	// Create the logical device for the given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Logical Device!");
	}

	// Queues are created at the same time as the device..
	// So we want heandle to queues
	// From given logical device, of given Queue Family, of given Queue Index (0 since only one queue), place reference in given VkQueue
	vkGetDeviceQueue(mainDevice.logicalDevice, m_QueueFamilyIndices.transferFamily, 0, &m_TransferQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, m_QueueFamilyIndices.computeFamily, 0, &m_ComputeQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, m_QueueFamilyIndices.graphicsFamily, 0, &m_GraphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, m_QueueFamilyIndices.presentationFamily, 0, &m_PresentationQueue);
}

void VulkanRenderer::SetupDebugMessnger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	VkResult result = CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to set up debug messenger!");
	}
}

void VulkanRenderer::CreateSurface()
{
	// https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Window_surface
	// Create Surface (creating a surface create info struct, runs the create surface function, returns result)
	VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &m_SwapchainSurface));
}

void VulkanRenderer::CreateSwapChain()
{
	// Get swap chain details so we can pick best settings.
	SwapChainDetails swapChainDetails = GetSwapChainDetails(mainDevice.physicalDevice);

	// Find optimal surface values for our swap chain
	// 1. choose best surface format
	VkSurfaceFormatKHR surfaceFormat = ChooseBestSurfaceFormat(swapChainDetails.formats);
	// 2. choose best presentation mode
	VkPresentModeKHR presentationMode = ChooseBestPresentationMode(swapChainDetails.presentationModes);
	// 3. choose swap chain image size
	VkExtent2D extent = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in the swap chain? Get 1 more than the minimum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	// if imageCount higher than max, then clamp down to max
	// if 0, then limitless.
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = m_SwapchainSurface;														// Swapchain surface
	swapChainCreateInfo.imageFormat = surfaceFormat.format;										// SwapChain Format
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;								// swapchain colorspace
	swapChainCreateInfo.presentMode = presentationMode;											// swapchain presentation mode
	swapChainCreateInfo.imageExtent = extent;													// swapchain image extents
	swapChainCreateInfo.minImageCount = imageCount;												// minimum images in swapchain
	swapChainCreateInfo.imageArrayLayers = 1;													// number of layers for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						// what attachment images will be used as
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;	// Transform to perform on swap chain images
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle blending images with external graphics (e.g. other windows)
	swapChainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of image not in view (e.g. behind another window, off screen, etc)

	// If graphics and presentation familes are different, then swapchain must let images be shared between familes.
	if (m_QueueFamilyIndices.graphicsFamily != m_QueueFamilyIndices.presentationFamily)
	{
		// Queues to share between
		uint32_t queueFamilyIndices[] = {
			(uint32_t)m_QueueFamilyIndices.graphicsFamily,
			(uint32_t)m_QueueFamilyIndices.presentationFamily
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;		// image share handling
		swapChainCreateInfo.queueFamilyIndexCount = 2;							// number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;			// array of queues to share between
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	// if old swapchain been destoryed and this one replaces it, then link old one to quickly hand over responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// create swapchain
	VK_CHECK(vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &m_Swapchain));

	// store for later reference
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// Get swap chain images(first count, then values)
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, m_Swapchain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, m_Swapchain, &swapChainImageCount, images.data());

	for (VkImage image : images)
	{
		// Store the image handle
		SwapChainImage swapChainImage = {};
		swapChainImage.image = image;
		VkUtils::CreateImageView(mainDevice.logicalDevice, image, &swapChainImage.imageView, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		// Store the depth image handle and depth image memory
		SwapChainImage depthStencilImage = {};
		VkDeviceMemory depthStencilImageMemory = {};
		VkUtils::CreateImage2D(mainDevice.logicalDevice, mainDevice.physicalDevice,
			swapChainExtent.width, swapChainExtent.height, &depthStencilImageMemory, &depthStencilImage.image,
			VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
		VkUtils::CreateImageView(mainDevice.logicalDevice, depthStencilImage.image, &depthStencilImage.imageView,
			VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

		// Add to swapchain image list
		m_SwapChainImages.push_back(swapChainImage);
		m_SwapchainDepthStencilImages.push_back(depthStencilImage);
		m_SwapchainDepthStencilImageMemories.push_back(depthStencilImageMemory);
	}

	// Create Swapchain FrameBuffers
	m_SwapChainFramebuffers.resize(m_SwapChainImages.size());

	// Create a framebuffer for each swapchain image
	for (size_t i = 0; i < m_SwapChainFramebuffers.size(); ++i)
	{
		std::array<VkImageView, 2> attachments =
		{
			m_SwapChainImages[i].imageView,
			m_SwapchainDepthStencilImages[i].imageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;											// Render pass layout the framebuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();								// List of attachments (1:1 with render pass)
		framebufferCreateInfo.width = swapChainExtent.width;									// framebuffer width
		framebufferCreateInfo.height = swapChainExtent.height;									// framebuffer height
		framebufferCreateInfo.layers = 1;														// framebuffer layers

		VK_CHECK(vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &m_SwapChainFramebuffers[i]));
	}
}

void VulkanRenderer::CreateRenderPass()
{
	CreateBasicRenderPass();
	CreateOffScreenRenderPass();
}

void VulkanRenderer::CreateBasicRenderPass()
{
	// Array of Subpasses
	std::array<VkSubpassDescription, 1> subpasses{};

	// ATTACHMENTS
	// SUBPASS 1 ATTACHMENTS (INPUT ATTACHMEMNTS)
	// Colour Attachment
	VkAttachmentDescription colourAttachment = {};
	colourAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
	colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthStencilAttachment = {};
	depthStencilAttachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
	depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colourAttachmentRef = {};
	colourAttachmentRef.attachment = 0;
	colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthStencilAttachmentRef = {};
	depthStencilAttachmentRef.attachment = 1;
	depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Set up Subpass 1
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colourAttachmentRef;
	subpasses[0].pDepthStencilAttachment = &depthStencilAttachmentRef;

	// Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies;

	// Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after ..
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;					// �ܺο��� �����Ƿ�, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;			// Stage access mask (memory access)
	// But most happen before ..
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Subpass 1 layout (colour/depth) to Externel(image/image)
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL; // We do not link swapchain. So, dstStageMask is to be SHADER_BIT.
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	std::array<VkAttachmentDescription, 3> renderPassAttachments = { colourAttachment, depthStencilAttachment };

	// Create info for Render Pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses = subpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VK_CHECK(vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &m_BasicRenderPass));
}

void VulkanRenderer::CreateOffScreenRenderPass()
{
	// Array of Subpasses
	std::array<VkSubpassDescription, 1> subpasses{};

	//
	// ATTACHMENTS
	// 
	// SwapChain Colour attacment of render pass
	VkAttachmentDescription swapChainColourAttachment = {};
	swapChainColourAttachment.format = swapChainImageFormat;							// format to use for attachment
	swapChainColourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;						// number of samples to write for multisampling, relative to multisampling
	swapChainColourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					// Describes what to do with attachment before rendering
	swapChainColourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				// Describes what to do with attachment after rendering
	swapChainColourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;		// Describes what to do with stencil before rendering
	swapChainColourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		// Describes what to do with stencil after rendering

	// FrameBuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations, initial -> subpass -> final
	swapChainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;				// Image data layout before render pass starts
	swapChainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;			// Image data layout after render pass (to change to)

	VkAttachmentDescription depthStencilAttachment = {};
	depthStencilAttachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
	depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// REFERENCES
	// FrameBuffer�� ���� �� ����Ͽ���, attachment �迭�� �����Ѵٰ� ���� �ȴ�.
	// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference swapChainColourAttachmentRef = {};
	swapChainColourAttachmentRef.attachment = 0;	// �󸶳� ���� attachment�� �ִ��� ����X, ���° attachment�� �����ϳ��� ����.
	swapChainColourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthStencilAttachmentRef = {};
	depthStencilAttachmentRef.attachment = 1;
	depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// NOTE: We don't need Input Reference, because we don't use more than two subpasses.
	// Information about a particular subpass the render pass is using
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;			// Pipeline type subpass is to be bound to
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &swapChainColourAttachmentRef;
	subpasses[0].pDepthStencilAttachment = &depthStencilAttachmentRef;

	//
	// SUBPASS DEPENDENCY
	//
	// Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies;

	// Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after ..
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;					// �ܺο��� �����Ƿ�, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;			// Stage access mask (memory access)
	// But most happen before ..
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after ..
	subpassDependencies[1].srcSubpass = 1;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Pipeline stage
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// But most happen before ..
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	//
	// RENDER PASS CREATE INFO
	//
	std::array<VkAttachmentDescription, 2> renderPassAttachments = { swapChainColourAttachment, depthStencilAttachment };

	// Create info for Render Pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses = subpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VK_CHECK(vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &m_OffScreenRenderPass));
}

void VulkanRenderer::CreateOffScrrenDescriptorSet()
{
	// CREATE INPUT ATTACHMENT IMAGE DESCRIPTOR SET LAYOUT
	VkDescriptorImageInfo colourAttachmentDescriptor = {};
	colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	colourAttachmentDescriptor.imageView = m_ColourBufferImageView;
	colourAttachmentDescriptor.sampler = VK_NULL_HANDLE;

	VkDescriptorImageInfo depthAttachmentDescriptor = {};
	depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthAttachmentDescriptor.imageView = m_DepthBufferImageView;
	depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

	VkUtils::DescriptorBuilder inputOffScreen = {};
	inputOffScreen.Begin(m_pLayoutCache.get(), m_pDescriptorAllocator.get());
	inputOffScreen.BindImage(0, &colourAttachmentDescriptor, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputOffScreen.BindImage(1, &depthAttachmentDescriptor, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_DescriptorManager.AddDescriptorSet(&inputOffScreen, ToWideString("OffScreen"));
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	/// UNIFORM VALUES DESCRIPTOR SET LAYOUT
	// uboViewProjection Binding Info
	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	vpLayoutBinding.binding = 0;											// Binding point in shader (designated by binding number of shader)
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// Type of Descriptor ( uniform, dynamic uniform, etc)
	vpLayoutBinding.descriptorCount = 1;									// Number of decriptors for binding
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// Shader stage to bind to
	vpLayoutBinding.pImmutableSamplers = nullptr;							// For texture : Can make sampler data unchanged (immutable) by specifying in layout
	// Model Binding Info
	/*VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;*/

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding };

	// Create Descriptor Set Layout with given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutCreateInfo.pBindings = layoutBindings.data();

	// Create Descriptor Set layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to Create Descriptor set layout!");
	}

	/// CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT
	// texture binding info
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	// Create a Descriptor Set Layout with given bindings for texture
	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to Create Descriptor set layout!");
	}
}

void VulkanRenderer::CreatePushConstantRange()
{
	// Define Push constant values(no 'create' needed)
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	// Shader stage push constant will go to
	pushConstantRange.offset = 0;								// Offset into given data to pass to push constant
	pushConstantRange.size = sizeof(Model);						// Size of Data Being Passed
}

void VulkanRenderer::CreateOffScreenPipeline()
{
	// Create Second Pass Pipeline
	// Second pass shaders
	auto vertexShaderCode = VkUtils::ReadFile("Shaders/second_vert.spv");
	auto fragmentShaderCode = VkUtils::ReadFile("Shaders/second_frag.spv");

	// Build Shaders
	VkShaderModule vertexShaderModule = VkUtils::CreateShaderModule(mainDevice.logicalDevice, vertexShaderCode);
	VkShaderModule fragmentShaderModule = VkUtils::CreateShaderModule(mainDevice.logicalDevice, fragmentShaderCode);

	// Set new shaders
	// Vertex Stage Creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;						// Shader stage name
	vertexShaderCreateInfo.module = vertexShaderModule;								// Shader moudle to be used by stage
	vertexShaderCreateInfo.pName = "main";											// Entry point in to shader
	// Fragment Stage Creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;					// Shader stage name
	fragmentShaderCreateInfo.module = fragmentShaderModule;							// Shader moudle to be used by stage
	fragmentShaderCreateInfo.pName = "main";										// Entry point in to shader

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// -- VERTEX INPUT --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	// No Vertex data for OffScreen pass
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	// -- INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// List versus Strip: ���ӵ� ��(Strip), �� �� ��� (List)
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		// Primitive type to assemble vertices
	inputAssembly.primitiveRestartEnable = VK_FALSE;					// Allow overriding of "strip" topology to start new primitives

	// -- VIPEPORT & SCISSOR --
	// Create a viewport info struct
	VkViewport viewport = {};
	viewport.x = 0.0f;								// x start coordinate
	viewport.y = 0.0f;								// y start coordinate
	viewport.width = (float)swapChainExtent.width;	// width of viewport
	viewport.height = (float)swapChainExtent.height;// height of viewport
	viewport.minDepth = 0.0f;						// min framebuffer depth
	viewport.maxDepth = 1.0f;						// max framebuffer depth

	// Create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };						// offset to use region from
	scissor.extent = swapChainExtent;				// extent to describe region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;			// Change if fragments beyond near/far planes are clipped (default) or clamped to plane, you can only use this to accept depthBiasClamp of physical device VK_TRUE
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;	// Whether tp discard data and skip rasterizer. Never creates fragments only suitable for pipline without framebuffer output.
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	// How to handle filling points between vertices.
	rasterizerCreateInfo.lineWidth = 1.0f;						// How thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;		// Which face of a tri to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// Winding to determine which side in front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;			// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping

	// -- MULTISAMPLING --
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;						// Enable multisample shading or not
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;		// Number of samples to use per fragment (e.g. 4xMSAA, 8xMSAA)

	// -- BLENDING --
// Blending decides how to blend a new colour being written to a fragment, with the old value

// Blend Attacment State (how blending is handled)
	VkPipelineColorBlendAttachmentState colourState = {};
	colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;					// Colours to apply blending to
	colourState.blendEnable = VK_TRUE;											// Enable Blending

	// Blending uses equation: (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
	colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;				// if it is 0.3
	colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		// this is (1.0 - 0.3)
	colourState.colorBlendOp = VK_BLEND_OP_ADD;
	// Summarised: (SRC_ALPHA * new colour) + (MINUS_SRC_ALPHA * old colour) == (alpha * new ) + (1 - alpha * old)

	colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colourState.alphaBlendOp = VK_BLEND_OP_ADD;
	// Summarised: (1 * new alpha) + (0 * old alpha) == new alpha

	VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
	colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlendingCreateInfo.logicOpEnable = VK_FALSE;							// Alternative to calculations is to use logical operations
	colourBlendingCreateInfo.attachmentCount = 1;
	colourBlendingCreateInfo.pAttachments = &colourState;

	// -- PIPELINE LAYOUT (It's like Root signature in D3D12) --
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorManager.GetVkDescriptorSetLayout(ToWideString("OffScreen"));
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;


	VK_CHECK(vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &secondPipelineLayout));

	// Don't want to write to depth buffer
	// -- DEPTH STENCIL TESTING --
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;				// Enable checking depth to determine fragment wrtie
	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;				// Enable writing to depth buffer (to replace old values)
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;		// Comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		// Depth Bounds Test: Does the depth value exist between two bounds, �� �ȼ��� ���� ���� Ư�� ���� �ȿ� �ִ����� üũ�ϴ� �˻�
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;			// Enable Stencil Test

	// -- GRAPHICS PIPELINE CREATION --
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;										// Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;								// List of shader stages
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;			// All the fixed function pipeline states
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;								// Pipeline Laytout pipeline should use
	pipelineCreateInfo.renderPass = renderPass;								// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;											// Subpass of render pass to use with pipeline

	// Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	// Existing pipline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;				// or index of pipeline being created to derive from (in case createing multiple at once)

	VK_CHECK(vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline));

	// Destroy second shader modules
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulkanRenderer::CreateCommandPool()
{
	// Get Indices of queue familes from device
	QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;			// Queue family type that buffers from this command pool will use

	// Create a Graphics Queue family Command Pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Command Pool!");
	}

}

void VulkanRenderer::CreateCommandBuffers()
{
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;			// �ش� ť �йи��� ť������ Ŀ�ǵ� ť ������ ���డ���ϴ�.
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to queue. cant be called by other buffers
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via 'vkCmdExecuteCommands' when recording commands in primary buffer.
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	// Allocate command buffers and place handles in array of buffers
	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Command Buffers!");
	}
}

void VulkanRenderer::CreateSynchronisation()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creataion information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	// Fence creataion information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]))
		{
			throw std::runtime_error("Failed to create a Semaphore and/or Fence!");
		}
	}
}

void VulkanRenderer::CreateTextureSampler()
{
	// Sampler Creation Info
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;						// How to render when image is magnified on screen
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;						// How to render when image is minified on screen
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in U(x) direction
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in U(y) direction
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in U(z) direction
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;	// Border beyond texture (only works for border clamp)
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;				// Whether coords should be normalized (between 0 and 1)
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;		// Mipmap interpolation mode
	samplerCreateInfo.mipLodBias = 0.0f;								// Level of Detatils bias for mip level
	samplerCreateInfo.minLod = 0.0f;									// Minimum Level of Detail to pick mip level
	samplerCreateInfo.maxLod = 0.0f;									// Maximum Level of Detail to pick mip level
	samplerCreateInfo.anisotropyEnable = VK_TRUE;						// Enable Anisotropy
	samplerCreateInfo.maxAnisotropy = 16;								// Anisotropy sample level

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a texture sampler!");
	}
}

void VulkanRenderer::CreateUniformBuffers()
{
	// ViewProjection Buffer Size
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

	//// Model Buffer size
	//VkDeviceSize modelBufferSize = modelUniformAligment * MAX_OBJECTS;

	// One uniform buffer for each image (and by extension, command buffer)
	vpUniformBuffer.resize(m_SwapChainImages.size());				// Uniform buffer�� ���̴����� ����ϴ� �����͸� �����ϴ� �����ε�, �� ����ü�� �̹������� �ϳ��� �ʿ�� �Ѵ�.
	vpUniformBufferMemory.resize(m_SwapChainImages.size());		// �ֳ��ϸ�, �� ����ü�� �̹������� ������ commandbuffer�� ����� �������� �����Ѵ�. 
	modelDUniformBuffer.resize(m_SwapChainImages.size());			// submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	modelDUniformBufferMemory.resize(m_SwapChainImages.size());	// �׷��� ������, ���ü� ������ �߻����� �ʱ� ���� m_SwapChainImages�� ���ڸ�ŭ �����ϴ� ���̴�.

	// Create Uniform buffers
	for (size_t i = 0; i < m_SwapChainImages.size(); ++i)
	{
		CreateBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMemory[i]);

		/*CreateBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDUniformBuffer[i], &modelDUniformBufferMemory[i]);*/
	}
}

void VulkanRenderer::CreateDescriptorPool()
{
	/// CREATE UNIFORM DESCRIPTOR POOL
	// Type of descriptors + How many DESCRIPTORS, not Descriptor Sets (combined makes the pool Size)
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());	// descriptor pool���� ������ �� �ִ� descriptor set�� �� ������ ����.

	// Model Poll (DYNAMIC)
	VkDescriptorPoolSize modelPoolSize = {};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDUniformBuffer.size());

	// List of Pool sizes
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { vpPoolSize };

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(m_SwapChainImages.size());		// Maximum numbeer of Descriptor Sets that can be created from pool
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());										// Amount of Poool Sizes being passed
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();										// Pool Sizes to create pool with

	// Create Descriptor Pool
	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Uniform Descriptor Pool");
	}

	/// CREATE SAMPLER DESCRIPTOR POOL
	// Texture Sampler Pool
	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS;

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Sampler Descriptor Pool");
	}

	// CREATE INPUT ATTACHMENT DESCRIPTOR POOL
	// Colour attachment pool size
	VkDescriptorPoolSize colourInputPoolSize = {};
	colourInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colourInputPoolSize.descriptorCount = static_cast<uint32_t>(m_ColourBufferImageView.size());
	// depth attachment pool size
	VkDescriptorPoolSize depthInputPoolSize = {};
	depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

	std::vector<VkDescriptorPoolSize> inputPoolSizes = { colourInputPoolSize, depthInputPoolSize };

	VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
	inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	inputPoolCreateInfo.maxSets = m_SwapChainImages.size();
	inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
	inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Sampler Descriptor Pool");
	}
}

void VulkanRenderer::CreateDescriptorSets()
{
	// Resize Descriptor Set list so one for every buffer
	descriptorSets.resize(m_SwapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(m_SwapChainImages.size(), descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;										// Pool to allocate Descriptor Set from
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(m_SwapChainImages.size());		// Number of sets to allocate
	setAllocInfo.pSetLayouts = setLayouts.data(); // Layouts to use to allocate sets (1 to 1 relationship)

	// Allocate Descriptor Sets (multiple)
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Descriptor sets");
	}

	// Update all of descriptor set buffer bindings
	for (size_t i = 0; i < m_SwapChainImages.size(); ++i)
	{
		// VIEW PROJECTION DESCRIPTOR
		// Buffer Info and data offset info
		VkDescriptorBufferInfo uboViewProjectionBufferInfo = {};
		uboViewProjectionBufferInfo.buffer = vpUniformBuffer[i];	// Buffer to get data from
		uboViewProjectionBufferInfo.offset = 0;					// Position of start of data
		uboViewProjectionBufferInfo.range = sizeof(UboViewProjection);			// size of data

		// Data about connection between binding and buffer
		VkWriteDescriptorSet uboViewProjectionSetWrite = {};
		uboViewProjectionSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboViewProjectionSetWrite.dstSet = descriptorSets[i];		// Descriptor Set to Update
		uboViewProjectionSetWrite.dstBinding = 0;					// Binding to update (matches with binding on layout/shader)
		uboViewProjectionSetWrite.dstArrayElement = 0;			// Index in array to update
		uboViewProjectionSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// Type of Descriptor
		uboViewProjectionSetWrite.descriptorCount = 1;								// Amount of Update
		uboViewProjectionSetWrite.pBufferInfo = &uboViewProjectionBufferInfo;						// Information about buffer data to bind

		// MODEL DESCRIPTOR
		/*VkDescriptorBufferInfo modelBufferInfo = {};
		modelBufferInfo.buffer = modelDUniformBuffer[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = modelUniformAligment;

		VkWriteDescriptorSet modelSetWrite = {};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.pBufferInfo = &modelBufferInfo;*/

		// List of Descriptor Set Writes
		std::vector<VkWriteDescriptorSet> setWrites = { uboViewProjectionSetWrite };

		// Update the descriptor Sets with new buffer/binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateInputDescriptorSets()
{
	inputDescriptorSets.resize(m_SwapChainImages.size());
	// Fill array of layouts ready for sets creation
	std::vector<VkDescriptorSetLayout> setLayouts(m_SwapChainImages.size(), inputDescriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = inputDescriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(m_SwapChainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	// Allocate Descriptor Sets (multiple)
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Input attachment Descriptor sets");
	}

	// Update each descriptor set with input attachment
	for (size_t i = 0; i < m_SwapChainImages.size(); ++i)
	{
		VkDescriptorImageInfo colourAttachmentDescriptor = {};
		colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		colourAttachmentDescriptor.imageView = m_ColourBufferImageView[i];
		colourAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet colourWrite = {};
		colourWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colourWrite.dstSet = inputDescriptorSets[i];
		colourWrite.dstBinding = 0;
		colourWrite.dstArrayElement = 0;
		colourWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		colourWrite.descriptorCount = 1;
		colourWrite.pImageInfo = &colourAttachmentDescriptor;

		VkDescriptorImageInfo depthAttachmentDescriptor = {};
		depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthAttachmentDescriptor.imageView = depthBufferImageView[i];
		depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet depthWrite = {};
		depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depthWrite.dstSet = inputDescriptorSets[i];
		depthWrite.dstBinding = 1;
		depthWrite.dstArrayElement = 0;
		depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthWrite.descriptorCount = 1;
		depthWrite.pImageInfo = &depthAttachmentDescriptor;

		std::vector<VkWriteDescriptorSet> setWrites = { colourWrite, depthWrite };

		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::UpdateUniformBuffers(uint32_t imageIndex)
{
	// Copy VP data
	void* pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &pData);
	memcpy(pData, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	//// Copy Model data
	//for (size_t i = 0; i < meshes.size(); ++i)
	//{
	//	Model* thisModel = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAligment));
	//	*thisModel = meshes[i].GetModel();
	//}

	//vkMapMemory(mainDevice.logicalDevice, modelDUniformBufferMemory[imageIndex], 0, modelUniformAligment * meshes.size(), 0, &pData);
	//memcpy(pData, modelTransferSpace, modelUniformAligment * meshes.size());
	//vkUnmapMemory(mainDevice.logicalDevice, modelDUniformBufferMemory[imageIndex]);

}

void VulkanRenderer::RecordCommands(uint32_t currentImage)
{
	// information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmitted when it has already been submitted and is awaiting execution

	// Information about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;								// Render Pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };							// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;					// Size of region to run render pass on (starting at offset)

	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].color = { 0.6f, 0.65f, 0.4f, 1.0f };
	clearValues[2].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();								// List of clear values
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];

	// Start recording commands to command buffer!
	VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a Command Buffer!");
	}

	//Begin Render Pass
	vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // ���� �н��� ������ ���� ��� ���ۿ� ����ϴ� ���� �ǹ�

	// Bind Pipeline to be used in render pass
	vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	for (size_t j = 0; j < modelList.size(); ++j)
	{
		MeshModel thisModel = modelList[j];

		// "Push" Constants to given shader stage directly (no buffer)
		vkCmdPushConstants(
			commandBuffers[currentImage],
			pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,										// Offset of push constants to update
			static_cast<uint32_t>(sizeof(Model)),	// Size of data being pushed
			&thisModel.GetModel());					// Actual data being pushed (can be array)


		for (size_t k = 0; k < thisModel.GetMeshCount(); ++k)
		{
			VkBuffer vertexBuffer[] = { thisModel.GetMesh(k)->GetVertexBuffer() };		// Buffers to bind
			VkDeviceSize offsets[] = { 0 };									// Offsets into buffers being bound
			vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffer, offsets);

			// Bind mesh index buffer, with 0 offset and using the uint32 type
			vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.GetMesh(k)->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			//// Dynamic Offset Amount
			//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAligment * j);
			std::array<VkDescriptorSet, 2> descriptorSetGroup = {
				descriptorSets[currentImage], samplerDescriptorSets[thisModel.GetMesh(k)->GetTexId()]
			};
			// Bind Descriptor Sets
			vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

			// Execute pipeline
			//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.GetVertexCount()), 1, 0, 0);
			vkCmdDrawIndexed(commandBuffers[currentImage], thisModel.GetMesh(k)->GetIndexCount(), 1, 0, 0, 0);
		}
	}

	// Start Second Subpass
	vkCmdNextSubpass(commandBuffers[currentImage], VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);

	vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout,
		0, 1, &inputDescriptorSets[currentImage], 0, nullptr);

	vkCmdDraw(commandBuffers[currentImage], 3, 1, 0, 0);

	// End Render Pass
	vkCmdEndRenderPass(commandBuffers[currentImage]);

	// Stop recording commands to command buffer!
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording a Command Buffer!");
	}

}

void VulkanRenderer::GetPhysicalDevice()
{
	// Enumerate Physical devices the vkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// If no devices avilable, then none suppotr Vulkan!
	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't find GPUs that support Vulkan Instance!");
	}

	// Get list  of Physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (CheckDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	//// Get properties of our new device
	//VkPhysicalDeviceProperties deviceProperties;
	//vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

void VulkanRenderer::AllocateDynamicBufferTransferSpace()
{
	//// Calculate alignment of model data
	//modelUniformAligment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

	//// Create space in memory to hold dynamic buffer that is aligend to our required alignment and holds MAX_OBJECTS
	//modelTransferSpace = (Model*)_aligned_malloc(modelUniformAligment * MAX_OBJECTS, modelUniformAligment);
}

bool VulkanRenderer::CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	// Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Create a list of VkExtensionPropeties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());


	// you can see outputs are VK_KHR_surface -> VK_KHR_win32_surface
	// VK_KHR_surface�� �÷����� �������� surface ���� ����� ���������ν� �÷����� ���ֹ��� �ʴ� �Ϲ����� �������̽��� ����
	// VK_KHR_win32_surface�� Windows���� Vulkan�� ����� �� �ִ� surface�� ����. ��, ù��°�� ���������� ����� �� �ִ� ����� �����ϰ� �ι�°�� �ش� �÷����� �°� ��ȣ�ۿ��� ���

	// Check if given extensions are in list of available extensions
	for (const auto& checkExtensions : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtensions, extension.extensionName) == 0)
			{
				//std::cout << checkExtensions << std::endl;
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	// Get Device Extension count
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0) return false;

	// Populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		// check for extension
		for (const auto& extension : extensions) {
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckValidationLayerSupport(const std::vector<const char*>* checkVaildationLayers)
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	// Create a list of VkInstanceLayerProperties using count
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// Check if all of layers exist in the availableLayes list.
	for (const char* layerName : *checkVaildationLayers)
	{
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckDeviceSuitable(VkPhysicalDevice device)
{
	/*
	// Information about the device itself (ID, name, type, vendor, etc)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	*/
	// Information about what the device can do (geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = GetQueueFamilies(device);

	bool extensionSupported = CheckDeviceExtensionSupport(device);

	bool swapChainValid = false;
	if (extensionSupported)
	{
		SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.isVaild() && extensionSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	// -- CAPABILITIES --
	// Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_SwapchainSurface, &swapChainDetails.surfaceCapabilities);

	// -- FORMATS --
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_SwapchainSurface, &formatCount, nullptr);
	// If formats returned, get list of formats
	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_SwapchainSurface, &formatCount, swapChainDetails.formats.data());
	}

	// -- PRESENTATION MODES --
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_SwapchainSurface, &presentationCount, nullptr);
	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_SwapchainSurface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

// Best format is subjective, but ours will be.
// format		: VK_FORMAT_R8G8B8A8_UNORM
// colorSpace	: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	// If only 1 format available and is undefined, then this means ALL formats are available(no restrictions)
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	// If restriceted, search for optimal format
	for (const auto& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
			&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	// If can't find optimal format, then just return first format.
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes)
{
	// Look for Mailbox presentation Mode
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR; // This spec has always to be available. (In vulkan)
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	// If current extent is at numeric limits, then extent can vary. Otherwise, it is the size of the window.
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		// If value cay vary, need to set manually

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// Create new extent using window size
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Surface also defines max and min, so make sure within boundaries by clamping value
		newExtent.width = std::clamp(newExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		newExtent.height = std::clamp(newExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

		return newExtent;
	}
}

VkFormat VulkanRenderer::ChooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	// Loop through options and find compatible one
	for (VkFormat format : formats)
	{
		// Get properties for give format on this device
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		// Depending on tiling choice, need to check for different bit flag
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a matching format!");
}

VkImage VulkanRenderer::CreateImage(
	uint32_t width, uint32_t height, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags useFlags,
	VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
	// CREATE IMAGE
	// Image creation info
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;					// Type of Image (1D, 2D or 3D)
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;									// Number of mipmap levels
	imageCreateInfo.arrayLayers = 1;								// Number of levels in image array
	imageCreateInfo.format = format;								// Format type of image
	imageCreateInfo.tiling = tiling;								// How image data should be "tiled" (arranged for optimal reading)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// Layout of image data on creation (�����ӹ��ۿ� �°� ������)
	imageCreateInfo.usage = useFlags;								// Bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;				// Number of samples for multi-sampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Whether image can be shared between queues

	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to Craete An Image");
	}

	// CRAETE MEMORY FOR IMAGE
	// Get memory requirements for a type of image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(mainDevice.physicalDevice, memRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memAllocInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to Allocate An Image memory");
	}

	// Connect memory to Image
	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

int VulkanRenderer::CreateTextureImage(std::string filename)
{
	// Load Image file
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc* imageData = LoadTextureFile(filename, &width, &height, &imageSize);

	// Create Staging Buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	CreateBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&imageStagingBuffer, &imageStagingBufferMemory);

	// Copy image data to staging buffer
	void* pData;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &pData);
	memcpy(pData, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	// Free Original image data
	stbi_image_free(imageData);

	// Create Image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;
	texImage = CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

	// Transition image to be DST for copy operation
	TransitionImageLayout(mainDevice.logicalDevice, m_GraphicsQueue, graphicsCommandPool, texImage,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// Copy Image data
	CopyImage(mainDevice.logicalDevice, m_GraphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);
	// Transition image to be shader readable for shader usage
	TransitionImageLayout(mainDevice.logicalDevice, m_GraphicsQueue, graphicsCommandPool, texImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Add texture data to vector for reference
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy Staging buffer
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	// Return index of new texture image
	return textureImages.size() - 1;
}

int VulkanRenderer::CreateTexture(std::string filename)
{
	// Create Texture Image and get its location in array
	int textureImageLoc = CreateTextureImage(filename);

	// Create Image View and Add to list
	VkImageView imageView = CreateImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	// Create Texture Descriptor
	int descriptorLoc = CreateTextureDescriptor(imageView);

	// Return Loc of set with texture
	return descriptorLoc;
}

int VulkanRenderer::CreateTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSet descriptorSet;

	// Descriptor Set Allocation Info
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerSetLayout;

	// Alloc Descriptor Sets
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Texture Descriptor Set!");
	}

	// texture image info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;	// Image Layout when is use
	imageInfo.imageView = textureImage;
	imageInfo.sampler = textureSampler;									// Sampler to use for set


	// Descriptor Wrtie Info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	// Return descriptor set loc
	return samplerDescriptorSets.size() - 1;
}

void VulkanRenderer::CreateMeshModel(std::string modelFile)
{
	// Importer model "Scene"
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	if (!scene)
	{
		throw std::runtime_error("Failed to load a model! (" + modelFile + ")");
	}

	// Get Vector of all materials with 1:1 ID placement
	std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

	// Conversion form the materials list IDs to our Descriptor array IDs
	std::vector<int> matToTex(textureNames.size());

	// Loop Over textureNames and create textures for them
	for (size_t i = 0; i < textureNames.size(); ++i)
	{
		// If material has no texture, set '0' to indicate no texzure, texture 0 will be reserved for a default texture
		if (textureNames[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			matToTex[i] = CreateTexture(textureNames[i]);
		}
	}

	// Load in all our meshes
	std::vector<Mesh> modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, m_GraphicsQueue, graphicsCommandPool,
		scene->mRootNode, scene, matToTex);

	// Create Mesh model and add to list
	MeshModel meshModel = MeshModel(modelMeshes);
	modelList.push_back(meshModel);
}

stbi_uc* VulkanRenderer::LoadTextureFile(std::string filename, int* width, int* height, VkDeviceSize* imageSize)
{
	// number of channels image uses
	int channels;

	// Load Pixel data for image
	std::string fileLoc = "Textures/" + filename;
	stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		throw std::runtime_error("Failed to Load a Texture file! (" + filename + ")");
	}

	// Calculate Image size using given and known data
	*imageSize = *width * *height * 4;

	return image;
}

void VulkanRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	// messageSeverity filed allows � �ɰ����� ���� debug�� ������ ���ΰ�. ��, �ɰ��� ������ ����
	createInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	// � ������ ����� �޼����� ���� ������ ����. GENERAL_BIT / VALIDATION_BIT / PERFORMANCE_BIT
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback; // ����� �޽����� ȣ���� Call back �Լ� ����
	createInfo.pUserData = nullptr; // Optional
}