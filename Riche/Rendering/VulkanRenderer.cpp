#include <random>

#include "VulkanRenderer.h"
#include "Core.h"
#include "Mesh.h"

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

int VulkanRenderer::Initialize(GLFWwindow* newWindow, Camera* newCamera)
{
	window = newWindow;
	camera = newCamera;

	try {
		CreateInstance();
		SetupDebugMessnger();
		CreateSurface();
		GetPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();

		CreateRenderPass();

		m_pDescriptorAllocator = std::make_shared<VkUtils::DescriptorAllocator>();
		m_pDescriptorAllocator->Initialize(mainDevice.logicalDevice);
		m_pLayoutCache = std::make_shared<VkUtils::DescriptorLayoutCache>();
		m_pLayoutCache->Initialize(mainDevice.logicalDevice);
		m_ResoucreManager.Initialize(mainDevice.logicalDevice, mainDevice.physicalDevice, m_TransferQueue, m_QueueFamilyIndices);

		CreatePushConstantRange();
		CreateCommandPool();
		CreateCommandBuffers();
		//AllocateDynamicBufferTransferSpace();
		CreateSynchronisation();

		m_ViewProjectionCPU.view = camera->View();
		m_ViewProjectionCPU.projection = camera->Proj();

		std::vector<BasicVertex> allMeshVertices;
		std::vector<uint32_t> allIndices;

		for (int i = 0; i < OBJECT_COUNT; ++i)
		{
			// Create a mesh
			// Vulkan의 viewport좌표계와 projection 행렬은 Y-Down
			// Clip Space와 NDC 공간도 기본적으로 Y-Down이다.
			// 정육면체의 정점 정의 (각 면을 개별적으로 정의)
			std::vector<BasicVertex> meshVertices = {
				// Front face
				{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
				{ {  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
				{ {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
				{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },

				// Back face
				{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
				{ {  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
				{ {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
				{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },

				// Left face
				{ { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
				{ { -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
				{ { -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
				{ { -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },

				// Right face
				{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
				{ { 0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
				{ { 0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
				{ { 0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },

				// Top face
				{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
				{ {  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
				{ {  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
				{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },

				// Bottom face
				{ { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
				{ {  0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
				{ {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
				{ { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }
			};

			// 정육면체의 인덱스 정의
			std::vector<uint32_t> meshIndices = {
				// Front face (CCW)
				0, 3, 2, 2, 1, 0,
				// Back face (CCW)
				4, 7, 6, 6, 5, 4,
				// Left face (CCW)
				8, 11, 10, 10, 9, 8,
				// Right face (CCW)
				12, 15, 14, 14, 13, 12,
				// Top face (CCW)
				16, 19, 18, 18, 17, 16,
				// Bottom face (CCW)
				20, 23, 22, 22, 21, 20
			};


			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_real_distribution<float> dis(-3.0f, 3.0f);
			std::uniform_real_distribution<float> disZ(-6.0f, 0.0f);
			Mesh mesh;
			float randomX = dis(gen);
			float randomY = dis(gen);
			float randomZ = disZ(gen);

			// 모델 행렬에 변환 적용
			mesh.GetModel() = glm::translate(glm::mat4(1.0f), glm::vec3(randomX, randomY, randomZ));
			m_ModelList.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(randomX, randomY, randomZ)));

			mesh.Initialize(meshVertices, meshIndices);
			AddDataToMiniBatch(g_MiniBatches, m_ResoucreManager, mesh);

			std::vector<glm::vec3> positions;
			for (const BasicVertex& vertex : meshVertices)
			{
				positions.push_back(vertex.pos);
			}

			AABB aabb = ComputeAABB(positions);
			aabb.min = mesh.GetModel() * aabb.min;
			aabb.max = mesh.GetModel() * aabb.max;

			entt::entity entity = m_Registry.create();
			m_Registry.emplace<Mesh>(entity, mesh);
			m_Registry.emplace<AABB>(entity, aabb);

			meshes.push_back(mesh);
		}
		FlushMiniBatch(g_MiniBatches, m_ResoucreManager);

		CreateUniformBuffers();
		CreateBasicFramebuffer();
		CreateSwapchainFrameBuffers();

		CreateDescriptorSets();
		CreateOffScrrenDescriptorSet();

		CreateBasicPipeline();
		CreateOffScreenPipeline();

		CreateBasicSemaphores();

		CreateInDirectDrawBuffer();
		CreateComputePipeline();
		editor.Initialize(window, instance, mainDevice.logicalDevice, mainDevice.physicalDevice, m_QueueFamilyIndices, m_GraphicsQueue);
	}
	catch (const std::runtime_error& e) {
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::UpdateModel(int modelId, glm::mat4 newModel)
{
	model.model = newModel;
	camera->Update();
	m_ViewProjectionCPU.view = camera->View();
	m_ViewProjectionCPU.projection = camera->Proj();
}

void VulkanRenderer::Draw()
{
	// 1. Get next available image to draw to and set something to signal when we're finished with the image (a semaphore)

	// Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, (std::numeric_limits<uint32_t>::max)());
	// Manually reset (close) fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	// -- Get Next Image --, Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;	// swapchain의 이미지 버퍼에서 index 값을 갖고온다.
	vkAcquireNextImageKHR(mainDevice.logicalDevice, m_Swapchain, (std::numeric_limits<uint32_t>::max)(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	vkWaitForFences(mainDevice.logicalDevice, 1, &m_BasicFence, VK_TRUE, (std::numeric_limits<uint32_t>::max)());
	vkResetFences(mainDevice.logicalDevice, 1, &m_BasicFence);

	FillBasicCommands();
	UpdateUniformBuffers(imageIndex);

	// RenderPass 1.
	VkSubmitInfo basicSubmitInfo = {};
	basicSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	basicSubmitInfo.waitSemaphoreCount = 1;									// Number of semaphores to wait on
	basicSubmitInfo.pWaitSemaphores = &imageAvailable[currentFrame];

	VkPipelineStageFlags basicWaitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	};
	basicSubmitInfo.pWaitDstStageMask = basicWaitStages;									// Stages to check semaphores at
	basicSubmitInfo.commandBufferCount = 1;											// Number of command buffers to submit
	basicSubmitInfo.pCommandBuffers = &m_BasicCommandBuffer;					// Command buffer to submit
	basicSubmitInfo.signalSemaphoreCount = 1;										// Number of semaphores to signal
	basicSubmitInfo.pSignalSemaphores = &m_BasicRenderAvailable;				// Semaphores to signal when command buffer finishes
	// Command buffer가 실행을 완료하면, Signaled 상태가 될 semaphore 배열.

	VkResult result = vkQueueSubmit(m_GraphicsQueue, 1, &basicSubmitInfo, m_BasicFence);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue");
	}

	RecordCommands(imageIndex);

	// 2. Submit command buffer to queue for execution, making sure it waits for the image to be signalled as available before drawing
	// and signals when it has finished rendering (wait a fence)
	// -- SUBMIT COOMAND BUUFER TO RENDER --, Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;									// Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &m_BasicRenderAvailable;			// List of semaphores to wait on, Command buffer가 실행하기전 대기해야하는 semaphores
	// 즉, 이 semphore가 signaled 상태가 될 때까지 대기하고, 그 후에 Command buffer를 실행한다.
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;									// Stages to check semaphores at
	submitInfo.commandBufferCount = 1;											// Number of command buffers to submit
	submitInfo.pCommandBuffers = &m_SwapchainCommandBuffers[imageIndex];					// Command buffer to submit
	submitInfo.signalSemaphoreCount = 1;										// Number of semaphores to signal
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];				// Semaphores to signal when command buffer finishes
	// Command buffer가 실행을 완료하면, Signaled 상태가 될 semaphore 배열.

// Submit command buffer to queue
	result = vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
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
	presentInfo.pSwapchains = &m_Swapchain;								// swapchain to present images to
	presentInfo.pImageIndices = &imageIndex;							// Index of images in swapchain to present

	result = vkQueuePresentKHR(m_PresentationQueue, &presentInfo);		// 렌더링 완료된 Graphics큐를 present 하는 함수. 즉, presentQueue로 전송한다고 볼 수 있음.
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

	editor.Cleanup();
	auto entityView = m_Registry.view<Mesh>();
	for (auto entity : entityView)
	{
		auto _mesh = entityView.get<Mesh>(entity);
		_mesh.Cleanup();
	}
	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);
	//_aligned_free(modelTransferSpace);
	m_ResoucreManager.Cleanup();

	vkDestroyImageView(mainDevice.logicalDevice, m_ColourBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, m_ColourBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, m_ColourBufferImageMemory, nullptr);

	vkDestroyImageView(mainDevice.logicalDevice, m_DepthBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, m_DepthBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, m_DepthBufferImageMemory, nullptr);

	vkDestroyFramebuffer(mainDevice.logicalDevice, m_BasicFramebuffer, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, m_OffScreenSetLayout, nullptr);

	vkDestroyBuffer(mainDevice.logicalDevice, m_ViewProjectionUBO, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, m_ViewProjectionUBOMemory, nullptr);

	vkDestroySemaphore(mainDevice.logicalDevice, m_BasicRenderAvailable, nullptr);
	vkDestroyFence(mainDevice.logicalDevice, m_BasicFence, nullptr);
	vkFreeCommandBuffers(mainDevice.logicalDevice, m_GraphicsCommandPool, 1, &m_BasicCommandBuffer);

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
		vkFreeCommandBuffers(mainDevice.logicalDevice, m_GraphicsCommandPool, 1, &m_SwapchainCommandBuffers[i]);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, m_GraphicsCommandPool, nullptr);

	//vkFreeCommandBuffers(mainDevice.logicalDevice, graphicsCommandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	for (auto framebuffer : m_SwapChainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, m_OffScreenPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, m_OffScreenPipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, m_OffScreenRenderPass, nullptr);

	vkDestroyPipeline(mainDevice.logicalDevice, m_BasicPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, m_BasicPipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, m_BasicRenderPass, nullptr);

	m_pLayoutCache->Cleanup();
	m_pDescriptorAllocator->Cleanup();

	for (auto image : m_SwapChainImages)
	{
		// VkImageView는 VkImage를 직접 참조해서 뷰를 생성한 것이므로, 호출해서 파괴해야한다.
		// 반면, VkImage는 스왑체인이 생성될 때 할당된 메모리 내에서 관리되므로 파괴할 필요가 없다.
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	for (int i = 0; i < m_SwapchainDepthStencilImages.size(); ++i)
	{
		vkDestroyImageView(mainDevice.logicalDevice, m_SwapchainDepthStencilImages[i].imageView, nullptr);
		vkDestroyImage(mainDevice.logicalDevice, m_SwapchainDepthStencilImages[i].image, nullptr);
		vkFreeMemory(mainDevice.logicalDevice, m_SwapchainDepthStencilImageMemories[i], nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, m_Swapchain, nullptr);
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

	// Information to create logical device (someties called device)
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());				// Number of queue create Infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();										// List of Queue Create Infos so device can craete required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());			// Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();									// List of enabled logical device extentions

	// Pyhsical Device Features the logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.depthBiasClamp = VK_FALSE;								// Rasterizer에서 해당 기능을 지원할 것인지에 대한 여부
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.multiDrawIndirect = VK_TRUE;
	deviceFeatures.drawIndirectFirstInstance = VK_TRUE;

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
}

void VulkanRenderer::CreateSwapchainFrameBuffers()
{
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
		framebufferCreateInfo.renderPass = m_OffScreenRenderPass;								// Render pass layout the framebuffer will be used with
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

void VulkanRenderer::CreateBasicFramebuffer()
{
	VkUtils::CreateImage2D(mainDevice.logicalDevice, mainDevice.physicalDevice, swapChainExtent.width, swapChainExtent.height,
		&m_ColourBufferImageMemory, &m_ColourBufferImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
	VkUtils::CreateImageView(mainDevice.logicalDevice, m_ColourBufferImage, &m_ColourBufferImageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	VkUtils::CreateImage2D(mainDevice.logicalDevice, mainDevice.physicalDevice,
		swapChainExtent.width, swapChainExtent.height, &m_DepthBufferImageMemory, &m_DepthBufferImage,
		VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
	VkUtils::CreateImageView(mainDevice.logicalDevice, m_DepthBufferImage, &m_DepthBufferImageView, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

	std::array<VkImageView, 2> attachments =
	{
		m_ColourBufferImageView,
		m_DepthBufferImageView
	};

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = m_BasicRenderPass;								// Render pass layout the framebuffer will be used with
	framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferCreateInfo.pAttachments = attachments.data();								// List of attachments (1:1 with render pass)
	framebufferCreateInfo.width = swapChainExtent.width;									// framebuffer width
	framebufferCreateInfo.height = swapChainExtent.height;									// framebuffer height
	framebufferCreateInfo.layers = 1;														// framebuffer layers

	VK_CHECK(vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &m_BasicFramebuffer));
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
	colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthStencilAttachment = {};
	depthStencilAttachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
	depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;					// 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
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
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL; // We do not link swapchain. So, dstStageMask is to be SHADER_BIT.
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	std::array<VkAttachmentDescription, 2> renderPassAttachments = { colourAttachment, depthStencilAttachment };

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

void VulkanRenderer::CreateBasicPipeline()
{
	// Create Second Pass Pipeline
// Second pass shaders
	auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/vert.spv");
	auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/frag.spv");

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
	// -- Create Pipeline --
// How the data for a single vertex (including info suach as position, colour, textuer coords, normals, etc) is as a whole. 
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;										// Can bind multiple streams of data, this defines which one
	bindingDescription.stride = static_cast<uint32_t>(sizeof(BasicVertex));	// size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;			// How to move between data after each vertex
	// VK_VERTEX_INPUT_RATE_VERTEX		: Move on to the next vertex
	// VK_VERTEX_INPUT_RATE_INSTANCE	: Move to a vertex for the next instance 

	// How the data for an attribute is defined whithin a vertex
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;
	// Position Attribute
	attributeDescriptions[0].binding = 0;								// Which binding the data is at (should be same as above)
	attributeDescriptions[0].location = 0;								// Location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;		// Format the data will take (also helps define size of data)
	attributeDescriptions[0].offset = offsetof(BasicVertex, pos);			// Where this attribute is defined in the data for a single vertex.
	// Colour Attribute
	attributeDescriptions[1].binding = 0;								// Which binding the data is at (should be same as above)
	attributeDescriptions[1].location = 1;								// Location in shader where data will be read from
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;		// Format the data will take (also helps define size of data)
	attributeDescriptions[1].offset = offsetof(BasicVertex, col);			// Where this attribute is defined in the data for a single vertex.
	// Texture Attribute
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(BasicVertex, tex);
	// -- VERTEX INPUT --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;					// List of Vertex binding descriptions (data spacing / stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();		// List of Vertex Attribute Descripitions ( data format and where to bind from)


	// -- INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// List versus Strip: 연속된 점(Strip), 딱 딱 끊어서 (List)
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
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_NONE;		// Which face of a tri to cull
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
	VkDescriptorSetLayout setLayouts[2] = { m_DescriptorManager.GetVkDescriptorSetLayout(ToWideString("ViewProjection")),
		m_DescriptorManager.GetVkDescriptorSetLayout(ToWideString("Model")) };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 2;
	pipelineLayoutCreateInfo.pSetLayouts = setLayouts;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = VK_NULL_HANDLE;


	VK_CHECK(vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_BasicPipelineLayout));

	// Don't want to write to depth buffer
	// -- DEPTH STENCIL TESTING --
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;				// Enable checking depth to determine fragment wrtie
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;				// Enable writing to depth buffer (to replace old values)
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;		// Comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		// Depth Bounds Test: Does the depth value exist between two bounds, 즉 픽셀의 깊이 값이 특정 범위 안에 있는지를 체크하는 검사
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
	pipelineCreateInfo.layout = m_BasicPipelineLayout;								// Pipeline Laytout pipeline should use
	pipelineCreateInfo.renderPass = m_BasicRenderPass;								// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;											// Subpass of render pass to use with pipeline

	// Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	// Existing pipline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;				// or index of pipeline being created to derive from (in case createing multiple at once)

	VK_CHECK(vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_BasicPipeline));

	// Destroy second shader modules
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulkanRenderer::CreateBasicSemaphores()
{
	// Semaphore creataion information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &m_BasicFence));
	VK_CHECK(vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_BasicRenderAvailable));
}

void VulkanRenderer::CreateInDirectDrawBuffer()
{
	std::vector<VkDrawIndexedIndirectCommand> indirectCommands;
	std::vector<AABB> aabbs;
	std::array<FrustumPlane, 6> cameraFrustumPlanes = CalculateFrustumPlanes(camera->ViewProj());

	auto entityView = m_Registry.view<Mesh, AABB>();
	for (auto entity : entityView)
	{
		auto [_mesh, _aabb] = entityView.get<Mesh, AABB>(entity);

		aabbs.push_back(_aabb);
	}

	VkDeviceSize indirectBufferSize = sizeof(VkDrawIndexedIndirectCommand) * g_MiniBatches.back().m_DrawIndexedCommands.size(); // * object count
	VkDeviceSize aabbBufferSize = sizeof(AABB) * aabbs.size();
	VkDeviceSize cameraPlaneSize = sizeof(FrustumPlane) * cameraFrustumPlanes.size();

	VkUtils::CreateBuffer(mainDevice.logicalDevice, mainDevice.physicalDevice, indirectBufferSize,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_InDirectDrawBuffer, &m_InDirectDrawBufferMemory);

	void* pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, m_InDirectDrawBufferMemory, 0, indirectBufferSize, 0, &pData);
	memcpy(pData, g_MiniBatches.back().m_DrawIndexedCommands.data(), indirectBufferSize);
	vkUnmapMemory(mainDevice.logicalDevice, m_InDirectDrawBufferMemory);

	VkUtils::CreateBuffer(mainDevice.logicalDevice, mainDevice.physicalDevice, aabbBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_BoundingBoxes, &m_BoundingBoxesMemory);

	pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, m_BoundingBoxesMemory, 0, aabbBufferSize, 0, &pData);
	memcpy(pData, aabbs.data(), aabbBufferSize);
	vkUnmapMemory(mainDevice.logicalDevice, m_BoundingBoxesMemory);

	VkUtils::CreateBuffer(mainDevice.logicalDevice, mainDevice.physicalDevice, cameraPlaneSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_CameraBoudingBox, &m_CameraBoudingBoxMemory);

	pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, m_CameraBoudingBoxMemory, 0, cameraPlaneSize, 0, &pData);
	memcpy(pData, cameraFrustumPlanes.data(), cameraPlaneSize);
	vkUnmapMemory(mainDevice.logicalDevice, m_CameraBoudingBoxMemory);

	VkDescriptorBufferInfo indirectBufferInfo = {};
	indirectBufferInfo.buffer = m_InDirectDrawBuffer;			// Buffer to get data from
	indirectBufferInfo.offset = 0;								// Position of start of data
	indirectBufferInfo.range = indirectBufferSize;			// size of data

	VkDescriptorBufferInfo aabbIndirectInfo = {};
	aabbIndirectInfo.buffer = m_BoundingBoxes;					// Buffer to get data from
	aabbIndirectInfo.offset = 0;								// Position of start of data
	aabbIndirectInfo.range = aabbBufferSize;					// size of data

	VkDescriptorBufferInfo cameraBoundingBoxInfo = {};
	cameraBoundingBoxInfo.buffer = m_CameraBoudingBox;			// Buffer to get data from
	cameraBoundingBoxInfo.offset = 0;								// Position of start of data
	cameraBoundingBoxInfo.range = cameraPlaneSize;			// size of data

	VkUtils::DescriptorBuilder indirect = VkUtils::DescriptorBuilder::Begin(m_pLayoutCache.get(), m_pDescriptorAllocator.get());
	indirect.BindBuffer(0, &indirectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	indirect.BindBuffer(1, &aabbIndirectInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	indirect.BindBuffer(2, &cameraBoundingBoxInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	m_DescriptorManager.AddDescriptorSet(&indirect, ToWideString("ViewFrustumCulling_COMPUTE"));
}

void VulkanRenderer::CreateComputePipeline()
{
	auto computeShaderCode = VkUtils::ReadFile("Resources/Shaders/viewfrustumculling_comp.spv");
	// Build Shaders
	VkShaderModule computeShaderModule = VkUtils::CreateShaderModule(mainDevice.logicalDevice, computeShaderCode);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_DescriptorManager.GetVkDescriptorSetLayout(ToWideString("ViewFrustumCulling_COMPUTE"));

	VK_CHECK(vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutInfo, nullptr, &m_ViewCullingComputePipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computePipelineCreateInfo.stage.module = computeShaderModule;
	computePipelineCreateInfo.stage.pName = "main";
	computePipelineCreateInfo.layout = m_ViewCullingComputePipelineLayout;

	VK_CHECK(vkCreateComputePipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_ViewCullingComputePipeline));
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
	// FrameBuffer를 만들 때 사용하였던, attachment 배열을 참조한다고 보면 된다.
	// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference swapChainColourAttachmentRef = {};
	swapChainColourAttachmentRef.attachment = 0;	// 얼마나 많은 attachment가 있는지 정의X, 몇번째 attachment를 참조하냐의 느낌.
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
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;					// 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// Pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;			// Stage access mask (memory access)
	// But most happen before ..
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after ..
	subpassDependencies[1].srcSubpass = 0;
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
	// CREATE INPUT ATTACHMENT IMAGE DESCRIPTOR SET LAYOUT
	VkDescriptorImageInfo colourAttachmentDescriptor = {};
	colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	colourAttachmentDescriptor.imageView = m_ColourBufferImageView;
	colourAttachmentDescriptor.sampler = textureSampler;

	VkDescriptorImageInfo depthAttachmentDescriptor = {};
	depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthAttachmentDescriptor.imageView = m_DepthBufferImageView;
	depthAttachmentDescriptor.sampler = textureSampler;

	VkUtils::DescriptorBuilder inputOffScreen = VkUtils::DescriptorBuilder::Begin(m_pLayoutCache.get(), m_pDescriptorAllocator.get());
	inputOffScreen.BindImage(0, &colourAttachmentDescriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputOffScreen.BindImage(1, &depthAttachmentDescriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_DescriptorManager.AddDescriptorSet(&inputOffScreen, ToWideString("OffScreenInput"));
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
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
	auto vertexShaderCode = VkUtils::ReadFile("Resources/Shaders/second_vert.spv");
	auto fragmentShaderCode = VkUtils::ReadFile("Resources/Shaders/second_frag.spv");

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
	// List versus Strip: 연속된 점(Strip), 딱 딱 끊어서 (List)
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
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_NONE;		// Which face of a tri to cull
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
	pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorManager.GetVkDescriptorSetLayout(ToWideString("OffScreenInput"));
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = VK_NULL_HANDLE;


	VK_CHECK(vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_OffScreenPipelineLayout));

	// Don't want to write to depth buffer
	// -- DEPTH STENCIL TESTING --
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;				// Enable checking depth to determine fragment wrtie
	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;				// Enable writing to depth buffer (to replace old values)
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;		// Comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		// Depth Bounds Test: Does the depth value exist between two bounds, 즉 픽셀의 깊이 값이 특정 범위 안에 있는지를 체크하는 검사
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
	pipelineCreateInfo.layout = m_OffScreenPipelineLayout;								// Pipeline Laytout pipeline should use
	pipelineCreateInfo.renderPass = m_OffScreenRenderPass;								// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;											// Subpass of render pass to use with pipeline

	// Pipeline Derivatives : can create multiple pipeline that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	// Existing pipline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;				// or index of pipeline being created to derive from (in case createing multiple at once)

	VK_CHECK(vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_OffScreenPipeline));

	// Destroy second shader modules
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_QueueFamilyIndices.graphicsFamily;			// Queue family type that buffers from this command pool will use

	// Create a Graphics Queue family Command Pool
	VK_CHECK(vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &m_GraphicsCommandPool));
}

void VulkanRenderer::CraeteSwapchainCommandPool()
{
}

void VulkanRenderer::CreateCommandBuffers()
{
	//
	// Swapchain Command Buffers (triple buffering)
	//
	m_SwapchainCommandBuffers.resize(m_SwapChainImages.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = m_GraphicsCommandPool;			// 해당 큐 패밀리의 큐에서만 커맨드 큐 동작이 실행가능하다.
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to queue. cant be called by other buffers
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via 'vkCmdExecuteCommands' when recording commands in primary buffer.
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(m_SwapChainImages.size());

	// Allocate command buffers and place handles in array of buffers
	VK_CHECK(vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, m_SwapchainCommandBuffers.data()));

	//
	// Single Command Buffer
	//
	cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = m_GraphicsCommandPool;			// 해당 큐 패밀리의 큐에서만 커맨드 큐 동작이 실행가능하다.
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// VK_COMMAND_BUFFER_LEVEL_PRIMARY		: buffer you submit directly to queue. cant be called by other buffers
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY	: buffer can't be called directly. Can be called from other buffers via 'vkCmdExecuteCommands' when recording commands in primary buffer.
	cbAllocInfo.commandBufferCount = 1;

	// Allocate command buffers and place handles in array of buffers
	VK_CHECK(vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, &m_BasicCommandBuffer));
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

void VulkanRenderer::CreateUniformBuffers()
{
	VkDeviceSize vpBufferSize = sizeof(ViewProjection);
	VkUtils::CreateBuffer(mainDevice.logicalDevice, mainDevice.physicalDevice, vpBufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&m_ViewProjectionUBO, &m_ViewProjectionUBOMemory);

	VkDeviceSize modelBufferSize = sizeof(glm::mat4) * m_ModelList.size();
	VkUtils::CreateBuffer(mainDevice.logicalDevice, mainDevice.physicalDevice, modelBufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&m_ModelListUBO, &m_ModelListUBOMemory);


}

void VulkanRenderer::CreateDescriptorSets()
{
	// VIEW PROJECTION DESCRIPTOR
	// Buffer Info and data offset info
	VkDescriptorBufferInfo uboViewProjectionBufferInfo = {};
	uboViewProjectionBufferInfo.buffer = m_ViewProjectionUBO;			// Buffer to get data from
	uboViewProjectionBufferInfo.offset = 0;								// Position of start of data
	uboViewProjectionBufferInfo.range = sizeof(ViewProjection);			// size of data

	VkUtils::DescriptorBuilder vpBuilder = VkUtils::DescriptorBuilder::Begin(m_pLayoutCache.get(), m_pDescriptorAllocator.get());
	vpBuilder.BindBuffer(0, &uboViewProjectionBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_DescriptorManager.AddDescriptorSet(&vpBuilder, ToWideString("ViewProjection"));

	VkDescriptorBufferInfo modelUBOInfo = {};
	modelUBOInfo.buffer = m_ModelListUBO;			// Buffer to get data from
	modelUBOInfo.offset = 0;								// Position of start of data
	modelUBOInfo.range = sizeof(glm::mat4) * m_ModelList.size();			// size of data

	VkUtils::DescriptorBuilder modelBulder = VkUtils::DescriptorBuilder::Begin(m_pLayoutCache.get(), m_pDescriptorAllocator.get());
	modelBulder.BindBuffer(0, &modelUBOInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_DescriptorManager.AddDescriptorSet(&modelBulder, ToWideString("Model"));
}

void VulkanRenderer::UpdateUniformBuffers(uint32_t imageIndex)
{
	// Copy VP data
	void* pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, m_ViewProjectionUBOMemory, 0, sizeof(ViewProjection), 0, &pData);
	memcpy(pData, &m_ViewProjectionCPU, sizeof(ViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, m_ViewProjectionUBOMemory);

	pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, m_ModelListUBOMemory, 0, sizeof(glm::mat4) * m_ModelList.size(), 0, &pData);
	memcpy(pData, m_ModelList.data(), sizeof(glm::mat4) * m_ModelList.size());
	vkUnmapMemory(mainDevice.logicalDevice, m_ModelListUBOMemory);

	std::array<FrustumPlane, 6> cameraFrustumPlanes = CalculateFrustumPlanes(camera->ViewProj());
	VkDeviceSize cameraPlaneSize = sizeof(FrustumPlane) * cameraFrustumPlanes.size();

	pData = nullptr;
	vkMapMemory(mainDevice.logicalDevice, m_CameraBoudingBoxMemory, 0, cameraPlaneSize, 0, &pData);
	memcpy(pData, cameraFrustumPlanes.data(), cameraPlaneSize);
	vkUnmapMemory(mainDevice.logicalDevice, m_CameraBoudingBoxMemory);

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
	FillOffScreenCommands(currentImage);
}

void VulkanRenderer::FillOffScreenCommands(uint32_t currentImage)
{
	// information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmitted when it has already been submitted and is awaiting execution

	// Information about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_OffScreenRenderPass;								// Render Pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };							// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;					// Size of region to run render pass on (starting at offset)

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();								// List of clear values
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = m_SwapChainFramebuffers[currentImage];

	// Start recording commands to command buffer!
	VK_CHECK(vkBeginCommandBuffer(m_SwapchainCommandBuffers[currentImage], &bufferBeginInfo));

	//Begin Render Pass
	vkCmdBeginRenderPass(m_SwapchainCommandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // 렌더 패스의 내용을 직접 명령 버퍼에 기록하는 것을 의미

	vkCmdBindPipeline(m_SwapchainCommandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_OffScreenPipeline);

	vkCmdBindDescriptorSets(m_SwapchainCommandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_OffScreenPipelineLayout,
		0, 1, &m_DescriptorManager.GetVkDescriptorSet(ToWideString("OffScreenInput")), 0, nullptr);

	vkCmdDraw(m_SwapchainCommandBuffers[currentImage], 3, 1, 0, 0);

	editor.RenderImGui(m_SwapchainCommandBuffers[currentImage]);

	// End Render Pass
	vkCmdEndRenderPass(m_SwapchainCommandBuffers[currentImage]);

	// Stop recording commands to command buffer!
	VK_CHECK(vkEndCommandBuffer(m_SwapchainCommandBuffers[currentImage]));
}

void VulkanRenderer::FillBasicCommands()
{
	// information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmitted when it has already been submitted and is awaiting execution

	// Information about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_BasicRenderPass;								// Render Pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };							// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;					// Size of region to run render pass on (starting at offset)

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.0f, 1.0f, 1.0f, 1.0f };
	clearValues[1].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();								// List of clear values
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = m_BasicFramebuffer;

	// Start recording commands to command buffer!
	VkResult result = vkBeginCommandBuffer(m_BasicCommandBuffer, &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a Command Buffer!");
	}

	//vkCmdBindPipeline(m_BasicCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ViewCullingComputePipeline);
	//vkCmdBindDescriptorSets(m_BasicCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
	//	m_ViewCullingComputePipelineLayout, 0, 1, &m_DescriptorManager.GetVkDescriptorSet(ToWideString("ViewFrustumCulling_COMPUTE")), 0, nullptr);

	//vkCmdDispatch(m_BasicCommandBuffer, OBJECT_COUNT, 1, 1);

	//Begin Render Pass
	vkCmdBeginRenderPass(m_BasicCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // 렌더 패스의 내용을 직접 명령 버퍼에 기록하는 것을 의미

	// Bind Pipeline to be used in render pass
	vkCmdBindPipeline(m_BasicCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BasicPipeline);

	//auto entityView = m_Registry.view<Mesh>();
	//for (auto entity : entityView)
	//{
	//	auto _mesh = entityView.get<Mesh>(entity);
	//	// "Push" Constants to given shader stage directly (no buffer)
	//	vkCmdPushConstants(
	//		m_BasicCommandBuffer,
	//		m_BasicPipelineLayout,
	//		VK_SHADER_STAGE_VERTEX_BIT,
	//		0,										// Offset of push constants to update
	//		static_cast<uint32_t>(sizeof(Model)),	// Size of data being pushed
	//		&_mesh.GetModel());					// Actual data being pushed (can be array)

	//	//VkDeviceSize offsets[] = { 0 };									// Offsets into buffers being bound
	//	//vkCmdBindVertexBuffers(m_BasicCommandBuffer, 0, 1, &_mesh.GetVkVertexBuffer(), offsets);

	//	//// Bind mesh index buffer, with 0 offset and using the uint32 type
	//	//vkCmdBindIndexBuffer(m_BasicCommandBuffer, _mesh.GetVkIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

	//	//// Dynamic Offset Amount
	//	//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAligment * j);
	//	// Bind Descriptor Sets
	//	vkCmdBindDescriptorSets(m_BasicCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//		m_BasicPipelineLayout, 0, 1, &m_DescriptorManager.GetVkDescriptorSet(ToWideString("ViewProjection")), 0, nullptr);

	//	// Execute pipeline
	//	//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.GetVertexCount()), 1, 0, 0);
	//	//vkCmdDrawIndexed(m_BasicCommandBuffer, _mesh.GetIndexCount(), 1, 0, 0, 0);
	//	//vkCmdDrawIndirect(m_BasicCommandBuffer, m_InDirectDrawBuffer, 0, 1, sizeof(VkDrawIndirectCommand));
	//	vkCmdDrawIndexedIndirect(m_BasicCommandBuffer, m_InDirectDrawBuffer, 0, 1, sizeof(VkDrawIndirectCommand));
	//}

	//
	// mini-batch system
	//
	for (auto miniBatch : g_MiniBatches)
	{
		//glm::mat4 model = glm::mat4(1.0f);
		//vkCmdPushConstants(
		//	m_BasicCommandBuffer,
		//	m_BasicPipelineLayout,
		//	VK_SHADER_STAGE_VERTEX_BIT,
		//	0,										// Offset of push constants to update
		//	static_cast<uint32_t>(sizeof(Model)),	// Size of data being pushed
		//	&model);					// Actual data being pushed (can be array)

		// Bind the vertex buffer with the correct offset
		VkDeviceSize vertexOffset = 0; // Always bind at offset 0 since indirect commands handle offsets
		vkCmdBindVertexBuffers(m_BasicCommandBuffer, 0, 1, &miniBatch.m_VertexBuffer, &vertexOffset);

		// Bind the index buffer with the correct offset
		vkCmdBindIndexBuffer(m_BasicCommandBuffer, miniBatch.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(m_BasicCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_BasicPipelineLayout, 0, 1, &m_DescriptorManager.GetVkDescriptorSet(ToWideString("ViewProjection")), 0, nullptr);

		vkCmdBindDescriptorSets(m_BasicCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_BasicPipelineLayout, 1, 1, &m_DescriptorManager.GetVkDescriptorSet(ToWideString("Model")), 0, nullptr);

		uint32_t drawCount = static_cast<uint32_t>(miniBatch.m_DrawIndexedCommands.size());
		VkDeviceSize indirectBufferSize = drawCount * sizeof(VkDrawIndexedIndirectCommand);

		vkCmdDrawIndexedIndirect(
			m_BasicCommandBuffer,
			m_InDirectDrawBuffer,
			0, // offset
			drawCount, // drawCount
			sizeof(VkDrawIndexedIndirectCommand) // stride
		);
	}

	// End Render Pass
	vkCmdEndRenderPass(m_BasicCommandBuffer);

	VkUtils::CmdImageBarrier(m_BasicCommandBuffer, m_ColourBufferImage,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	VkUtils::CmdImageBarrier(m_BasicCommandBuffer, m_DepthBufferImage,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

	// Stop recording commands to command buffer!
	VK_CHECK(vkEndCommandBuffer(m_BasicCommandBuffer));
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
	// VK_KHR_surface는 플랫폼에 독립적인 surface 생성 기능을 제공함으로써 플랫폼에 구애받지 않는 일반적인 인터페이스를 제공
	// VK_KHR_win32_surface는 Windows에서 Vulkan이 사용할 수 있는 surface를 생성. 즉, 첫번째는 공통적으로 사용할 수 있는 기능을 제공하고 두번째는 해당 플랫폼에 맞게 상호작용을 담당

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

	bool extensionSupported = CheckDeviceExtensionSupport(device);

	bool swapChainValid = false;
	if (extensionSupported)
	{
		SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	// Get the Queue family indices for the chosen physical device
	m_QueueFamilyIndices = VkUtils::GetQueueFamilies(device, m_SwapchainSurface);

	return m_QueueFamilyIndices.isVaild() && extensionSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
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
	if (surfaceCapabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
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

void VulkanRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	// messageSeverity filed allows 어떤 심각도에 대해 debug를 보여줄 것인가. 즉, 심각도 수준을 선택
	createInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	// 어떤 유형의 디버그 메세지를 받을 것인지 설정. GENERAL_BIT / VALIDATION_BIT / PERFORMANCE_BIT
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback; // 디버그 메시지를 호출할 Call back 함수 지정
	createInfo.pUserData = nullptr; // Optional
}