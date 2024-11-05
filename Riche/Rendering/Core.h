#pragma once

const int MAX_FRAME_DRAWS = 3;
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

inline bool VK_CHECK(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        throw std::exception();
    }
    return true;
}

inline VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << "\n" << std::endl;

	return VK_FALSE;
}