#pragma once

namespace VkUtils {
static VkFormat ChooseSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& formats, VkImageTiling tiling,
                                      VkFormatFeatureFlags featureFlags) {
  // Loop through options and find compatible one
  for (VkFormat format : formats) {
    // Get properties for give format on this device
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

    // Depending on tiling choice, need to check for different bit flag
    if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags) {
      return format;
    }
  }

  throw std::runtime_error("Failed to find a matching format!");
}

static VkDeviceAddress getVkDeviceAddress(VkDevice device, VkBuffer buffer) {
  VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {};
  bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
  bufferDeviceAddressInfo.buffer = buffer;

  VkDeviceAddress address = vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);
  return address;
}
}  // namespace VkUtils