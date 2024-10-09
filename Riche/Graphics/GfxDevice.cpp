#include "GfxDevice.h"

void GfxDevice::Initialize(GLFWwindow* newWindow)
{
	window = newWindow;

	CreateInstance();
	CreateSurface();
	GetPhysicalDeviceFromInstance();
	CreateLogicalDevice();
	CreateSwapchain();
}

void GfxDevice::Destroy()
{
	vkDestroySwapchainKHR(logicalDevice, swapchain.swapchain, nullptr);
	vkDestroySurfaceKHR(instance, swapchain.surface, nullptr);
	vkDestroyDevice(logicalDevice, nullptr);
	if (EnableDebug()) DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void GfxDevice::CreateInstance()
{
	// Most data here doens't affect the program and is for developer convenience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";				// Custom name of the application
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);	// Custion verstion of the application
	appInfo.pEngineName = "Jordan-Engine";					// Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);		// Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_3;

	// Creation Information for a VkInstance (Vulkan Instance)
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// layers need to be enabled by specifying their name. This name is standard validation.
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	bool enableValidationLayers = EnableDebug();

	if (enableValidationLayers && !CheckValidationLayerSupport(&validationLayers))
	{
		assert(false && "Valdation Layers requested, but not available");
	}

	// Setup debugger
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		// messageSeverity filed allows 어떤 심각도에 대해 debug를 보여줄 것인가. 즉, 심각도 수준을 선택
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		// 어떤 유형의 디버그 메세지를 받을 것인지 설정. GENERAL_BIT / VALIDATION_BIT / PERFORMANCE_BIT
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback; // 디버그 메시지를 호출할 Call back 함수 지정
		debugCreateInfo.pUserData = nullptr; // Optional

		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();
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
		assert(false && "VkInstance does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// Create Instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create a Vulkan Instance!");
	}

	result = CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to set up debug messenger!");
	}
}

void GfxDevice::CreateLogicalDevice()
{
	// Get the Queue family indices
	QueueFamilyIndices indices = GetQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> queueFamilyIndices = {
		indices.graphicsFamily.value(), indices.computeFamily.value(), indices.transferFamily.value(), indices.presentationFamily.value()
	};

	for (uint32_t idx : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = idx;					// The Index of the family to create a queue from
		queueCreateInfo.queueCount = 1;								// Number of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());				// Number of queue create Infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();										// List of Queue Create Infos so device can craete required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());			// Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();									// List of enabled logical device extentions

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.depthBiasClamp = VK_FALSE;								// Rasterizer에서 해당 기능을 지원할 것인지에 대한 여부
	deviceFeatures.samplerAnisotropy = VK_FALSE;

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;					// Physical device features logical device will use

	// Create the logical device for the given physical device
	VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Logical Device!");
	}

	graphicsQueueFamilyIndex = indices.graphicsFamily.value();
	vkGetDeviceQueue(logicalDevice, graphicsQueueFamilyIndex, 0, &grahpicsQueue);
	presentationQueueFamilyIndex = indices.presentationFamily.value();
	vkGetDeviceQueue(logicalDevice, presentationQueueFamilyIndex, 0, &presentationQueue);
	computeQueueFamilyIndex = indices.computeFamily.value();
	vkGetDeviceQueue(logicalDevice, computeQueueFamilyIndex, 0, &computeQueue);
	trasferQueueFamilyIndex = indices.transferFamily.value();
	vkGetDeviceQueue(logicalDevice, trasferQueueFamilyIndex, 0, &transferQueue);
}

void GfxDevice::GetPhysicalDeviceFromInstance()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// If no devices avilable, then none suppotr Vulkan!
	if (deviceCount == 0)
	{
		assert(false && "Can't find GPUs that support Vulkan Instance!");
	}

	// Get list  of Physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (CheckDeviceSuitable(device))
		{
			physicalDevice = device;
			break;
		}
	}

	// Get Properties of our new Device
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
	minBufferAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
}

void GfxDevice::CreateSurface()
{
	// https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Window_surface
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &swapchain.surface);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Craete Window Surface");
	}
}

void GfxDevice::CreateSwapchain()
{
	// Get swap chain details so we can pick best settings.
	SwapchainDetails swapchainDetails = GetSwapchainDetails(physicalDevice);

	// Find optimal surface values for our swap chain
	// 1. choose surface format
	VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	// 2. choose presentation mode
	VkPresentModeKHR presentationMode = VK_PRESENT_MODE_MAILBOX_KHR;
	// 3. choose swapchain image size
	VkExtent2D extent = ChooseSwapExtent(swapchainDetails.surfaceCapabilities);

	uint32_t imageCount = swapchainDetails.surfaceCapabilities.minImageCount + 1;

	// if imageCount higher than max, then clamp down to max
	// if 0, then limitless.
	if (swapchainDetails.surfaceCapabilities.maxImageCount > 0 && swapchainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapchainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = swapchain.surface;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.presentMode = presentationMode;
	swapchainCreateInfo.imageExtent = extent;
	swapchainCreateInfo.minImageCount = imageCount;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = swapchainDetails.surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;		// How to handle blending images with external graphics (e.g. other windows)
	swapchainCreateInfo.clipped = VK_TRUE;

	QueueFamilyIndices indices = GetQueueFamilies(physicalDevice);

	if (indices.graphicsFamily.value() != indices.presentationFamily.value())
	{
		uint32_t queueFamilyIndices[] = {
			indices.graphicsFamily.value(),
			indices.presentationFamily.value(),
		};

		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}
	// if old swapchain been destoryed and this one replaces it, then link old one to quickly hand over responsibilities
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create swapchain
	VkResult result = vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, nullptr, &swapchain.swapchain);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Create swapchain!");
	}

	swapchain.format = surfaceFormat.format;
	swapchain.extent = extent;
}

QueueFamilyIndices GfxDevice::GetQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices queueFamilyIndices{};

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());
	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilyList)
	{
		// First check if queue family has at least 1 queue in that family(could have no queues)
		// Queue can be multiple types defined through bitfield. need to bitwise AND with VK_QUEUE_*_BIT to check if has required type.
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamilyIndices.graphicsFamily = i; 	// if queue family is vaild, then get index
			queueFamilyIndices.graphicsQueueProperties = queueFamily;
		}

		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			queueFamilyIndices.computeFamily = i;
			queueFamilyIndices.computeQueueProperties = queueFamily;
		}

		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			queueFamilyIndices.transferFamily = i;
			queueFamilyIndices.transferQueueProperties = queueFamily;
		}

		// Check if Queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, swapchain.surface, &presentationSupport);
		// Check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			queueFamilyIndices.presentationFamily = i;;	// if queue family is vaild, then get index
			queueFamilyIndices.presentationQueueProperties = queueFamily;
		}

		if (queueFamilyIndices.isVaild())
		{
			break;
		}

		i++;
	}

	return queueFamilyIndices;
}

SwapchainDetails GfxDevice::GetSwapchainDetails(VkPhysicalDevice device)
{
	SwapchainDetails swapchainDetails;

	// -- CAPABILITIES --
	// Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, swapchain.surface, &swapchainDetails.surfaceCapabilities);

	// -- FORMATS --
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, swapchain.surface, &formatCount, nullptr);
	// If formats returned, get list of formats
	if (formatCount != 0)
	{
		swapchainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, swapchain.surface, &formatCount, swapchainDetails.formats.data());
	}

	// -- PRESENTATION MODES --
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, swapchain.surface, &presentationCount, nullptr);
	if (presentationCount != 0)
	{
		swapchainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, swapchain.surface, &presentationCount, swapchainDetails.presentationModes.data());
	}

	return swapchainDetails;
}

bool GfxDevice::CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	// Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Create a list of VkExtensionPropeties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

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

bool GfxDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device)
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

bool GfxDevice::CheckValidationLayerSupport(const std::vector<const char*>* checkVaildationLayers)
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

bool GfxDevice::CheckDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = GetQueueFamilies(device);

	bool extensionSupported = CheckDeviceExtensionSupport(device);

	bool swapchainValid = false;
	if (extensionSupported)
	{
		SwapchainDetails swapchainDetails = GetSwapchainDetails(device);
		swapchainValid = !swapchainDetails.presentationModes.empty() && !swapchainDetails.formats.empty();
	}

	return indices.isVaild() && extensionSupported && swapchainValid && deviceFeatures.samplerAnisotropy;
}

VkExtent2D GfxDevice::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
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

bool GfxDevice::EnableDebug()
{
#ifdef NDEBUG	// NDEBUG is C++ standard Macro.
	return false;
#else
	return true;
#endif
}
