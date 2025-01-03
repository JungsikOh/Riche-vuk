#pragma once

const int MAX_FRAME_DRAWS = 3;
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_shader_draw_parameters", "VK_EXT_descriptor_indexing",
    //"VK_EXT_separate_sampler_usage"
};

inline bool VK_CHECK(VkResult result) {
  if (result != VK_SUCCESS) {
    throw std::exception();
  }
  return true;
}

inline VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  std::cerr << "Validation layer: " << pCallbackData->pMessage << "\n" << std::endl;

  return VK_FALSE;
}

template <typename T>
std::vector<T*> GetDataPointers(std::vector<std::vector<T>>& allVectors) {
  std::vector<T*> pointers;
  for (auto& vec : allVectors) {
    pointers.push_back(vec.data());
  }
  return pointers;
}