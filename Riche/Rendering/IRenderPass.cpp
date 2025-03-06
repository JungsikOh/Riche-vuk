#include "IRenderPass.h"

IRenderPass::IRenderPass(VkDevice device, VkPhysicalDevice physicalDevice) {
  vkGetBufferDeviceAddressKHR =
      reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
  vkCreateAccelerationStructureKHR =
      reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
  vkDestroyAccelerationStructureKHR =
      reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
  vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
      vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
  vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
      vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
  vkCmdBuildAccelerationStructuresKHR =
      reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
  vkBuildAccelerationStructuresKHR =
      reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
  vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
  vkGetRayTracingShaderGroupHandlesKHR =
      reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
  vkCreateRayTracingPipelinesKHR =
      reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

  // 함수 로딩 확인
  if (!vkGetBufferDeviceAddressKHR || !vkCreateAccelerationStructureKHR || !vkDestroyAccelerationStructureKHR ||
      !vkGetAccelerationStructureBuildSizesKHR || !vkGetAccelerationStructureDeviceAddressKHR ||
      !vkCmdBuildAccelerationStructuresKHR || !vkBuildAccelerationStructuresKHR || !vkCmdTraceRaysKHR ||
      !vkGetRayTracingShaderGroupHandlesKHR || !vkCreateRayTracingPipelinesKHR) {
    throw std::runtime_error("Failed to load Vulkan Ray Tracing function pointers!");
  }

  rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
  VkPhysicalDeviceProperties2 deviceProperties2{};
  deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  deviceProperties2.pNext = &rayTracingPipelineProperties;
  vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

  accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  VkPhysicalDeviceFeatures2 deviceFeatures2{};
  deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures2.pNext = &accelerationStructureFeatures;
  vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

  vkCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT"));
}

VkDeviceAddress IRenderPass::GetVkDeviceAddress(VkDevice device, VkBuffer buffer) {
  VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {};
  bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
  bufferDeviceAddressInfo.buffer = buffer;

  VkDeviceAddress address = vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);
  return address;
}

VkStridedDeviceAddressRegionKHR IRenderPass::GetSbtEntryStridedDeviceAddressRegion(VkDevice device, VkBuffer buffer,
                                                                                   uint32_t handleCount) {
  const uint32_t handleSizeAligned = VkUtils::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize,
                                                          rayTracingPipelineProperties.shaderGroupHandleAlignment);
  VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegionKHR{};
  stridedDeviceAddressRegionKHR.deviceAddress = GetVkDeviceAddress(device, buffer);
  stridedDeviceAddressRegionKHR.stride = handleSizeAligned;
  stridedDeviceAddressRegionKHR.size = handleCount * handleSizeAligned;
  return stridedDeviceAddressRegionKHR;
}

void IRenderPass::CreateShaderBindingTable(VkDevice device, VkPhysicalDevice physicalDevice, ShaderBindingTable& shaderBindingTable,
                                           uint32_t handleCount) {
  shaderBindingTable.size = rayTracingPipelineProperties.shaderGroupHandleSize * handleCount;
  VkUtils::CreateBuffer(device, physicalDevice, shaderBindingTable.size,
                        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &shaderBindingTable.buffer,
                        &shaderBindingTable.memory, true);
  shaderBindingTable.stridedDeviceAddressRegion =
      GetSbtEntryStridedDeviceAddressRegion(device, shaderBindingTable.buffer, handleCount);
}

void IRenderPass::CreateAccelerationStructure(VkDevice device, VkPhysicalDevice physicalDevice,
                                              AccelerationStructure& accelerationStructure, VkAccelerationStructureTypeKHR type,
                                              VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo) {
  // Buffer and memory
  VkBufferCreateInfo bufferCreateInfo{};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));
  VkMemoryRequirements memoryRequirements{};
  vkGetBufferMemoryRequirements(device, accelerationStructure.buffer, &memoryRequirements);
  VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
  memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
  memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
  VkMemoryAllocateInfo memoryAllocateInfo{};
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex =
      VkUtils::FindMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
  VK_CHECK(vkBindBufferMemory(device, accelerationStructure.buffer, accelerationStructure.memory, 0));
  // Acceleration structure
  VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info{};
  accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  accelerationStructureCreate_info.buffer = accelerationStructure.buffer;
  accelerationStructureCreate_info.size = buildSizeInfo.accelerationStructureSize;
  accelerationStructureCreate_info.type = type;
  vkCreateAccelerationStructureKHR(device, &accelerationStructureCreate_info, nullptr, &accelerationStructure.handle);
  // AS device address
  VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
  accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  accelerationDeviceAddressInfo.accelerationStructure = accelerationStructure.handle;
  accelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);
}

void IRenderPass::CreateAccelerationStructureBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                                    AccelerationStructure& accelerationStructure,
                                                    VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo) {
  VkBufferCreateInfo bufferCreateInfo{};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));

  VkMemoryRequirements memoryRequirements{};
  vkGetBufferMemoryRequirements(device, accelerationStructure.buffer, &memoryRequirements);

  VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
  memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
  memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

  VkMemoryAllocateInfo memoryAllocateInfo{};
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex =
      VkUtils::FindMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
  VK_CHECK(vkBindBufferMemory(device, accelerationStructure.buffer, accelerationStructure.memory, 0));
}

ScratchBuffer IRenderPass::CreateScratchBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size) {
  ScratchBuffer scratchBuffer{};

  // Buffer and memory
  VkBufferCreateInfo bufferCreateInfo{};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &scratchBuffer.handle));

  VkMemoryRequirements memoryRequirements{};
  vkGetBufferMemoryRequirements(device, scratchBuffer.handle, &memoryRequirements);

  VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
  memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
  memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
  VkMemoryAllocateInfo memoryAllocateInfo = {};
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex =
      VkUtils::FindMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &scratchBuffer.memory));
  VK_CHECK(vkBindBufferMemory(device, scratchBuffer.handle, scratchBuffer.memory, 0));

  // Buffer device address
  VkBufferDeviceAddressInfoKHR bufferDeviceAddresInfo{};
  bufferDeviceAddresInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  bufferDeviceAddresInfo.buffer = scratchBuffer.handle;
  scratchBuffer.deviceAddress = vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddresInfo);

  return scratchBuffer;
}
