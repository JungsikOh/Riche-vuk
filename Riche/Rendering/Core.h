#pragma once
#define DESC_HANDLE uint64_t

const int MAX_FRAME_DRAWS = 3;
const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                   VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
                                                   VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                                                   VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                                   VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                                   VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                                   VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                                   VK_EXT_MESH_SHADER_EXTENSION_NAME};


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