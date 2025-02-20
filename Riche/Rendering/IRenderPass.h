#pragma once

#include "VkUtils/ChooseFunc.h"
#include "VkUtils/ResourceManager.h"

// Holds information for a ray tracing scratch buffer that is used as a temporary storage
struct ScratchBuffer {
  uint64_t deviceAddress = 0;
  VkBuffer handle = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
};

// Ray tracing acceleration structure
struct AccelerationStructure {
  VkAccelerationStructureKHR handle;
  uint64_t deviceAddress = 0;
  VkDeviceMemory memory;
  VkBuffer buffer;
};

struct ShaderBindingTable {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDeviceSize size;
  VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegion{};
};

class Camera;
class Editor;

class IRenderPass {
 public:
  PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
  PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
  PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
  PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
  PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
  PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
  PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
  PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
  PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
  PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};

  IRenderPass() = default;
  IRenderPass(VkDevice device, VkPhysicalDevice physicalDevice);
  ~IRenderPass() = default;

  virtual void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, Camera* camera,
                          Editor* editor, const uint32_t width, const uint32_t height) = 0;
  virtual void Cleanup() = 0;

  virtual void Update() = 0;

  virtual void Draw(VkSemaphore renderAvailable) = 0;

  VkDeviceAddress GetVkDeviceAddress(VkDevice device, VkBuffer buffer);
  VkStridedDeviceAddressRegionKHR GetSbtEntryStridedDeviceAddressRegion(VkDevice device, VkBuffer buffer, uint32_t handleCount);

  void CreateShaderBindingTable(VkDevice device, VkPhysicalDevice physicalDevice, ShaderBindingTable& shaderBindingTable,
                                uint32_t handleCount);

  void CreateAccelerationStructure(VkDevice device, VkPhysicalDevice physicalDevice, AccelerationStructure& accelerationStructure,
                                   VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
  void CreateAccelerationStructureBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                                         AccelerationStructure& accelerationStructure,
                                         VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

  ScratchBuffer CreateScratchBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size);

 private:
  // - Rendering Pipeline
  virtual void CreateFramebuffer() = 0;
  virtual void CreateRenderPass() = 0;

  virtual void CreatePipeline() = 0;
  virtual void CreatePipelineLayout() = 0;
  virtual void CreateBuffers() = 0;

  virtual void CreateCommandBuffers() = 0;
  virtual void RecordCommands() = 0;
};