#pragma once
#include "QueueFamilyIndices.h"
#include "Rendering/Core.h"
#include "Utils/Singleton.h"
#include "Utils/TextureUtils.h"

namespace VkUtils {

// TransferQueue를 이용해서 만들어야하는 Vulkan Object에 대해서 생성하는 클래스
class ResourceManager : public Singleton<ResourceManager> {
  friend class Singleton<ResourceManager>;

  VkDevice m_pDevice;
  VkPhysicalDevice m_pPhysicalDevice;
  QueueFamilyIndices m_queueFamilyIndices;

  VkFence m_fence = VK_NULL_HANDLE;

  void CreateFence();
  void CleanupFence();
  void CreateCommandPool();
  void CleanupCommandPool();

  void WaitForFenceValue();

 public:
  VkQueue m_transferQueue;
  VkQueue m_computeQueue;
  VkCommandPool m_transferCommandPool;
  VkCommandPool m_commputeCommandPool;
  VkCommandBuffer m_transferCommandBuffer;

  ResourceManager();
  ~ResourceManager();

  void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkQueue compute, QueueFamilyIndices indices);
  void Cleanup();

  VkCommandBuffer CreateAndBeginCommandBuffer();
  void CreateCommandBuffer();
  void BeginCommandBuffer();
  void EndAndSummitCommandBuffer(VkCommandBuffer commandbuffer);

  VkResult CreateVertexBuffer(uint32_t sizePerVertex, uint32_t vertexNum, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer,
                              void* pInitData);
  VkResult CreateVertexBuffer(uint32_t vertexDataSize, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer, void* pInitData);
  VkResult CreateIndexBuffer(uint32_t indexDataSize, VkDeviceMemory* pOutIndexBufferMemory, VkBuffer* pOutBuffer, void* pInitData);

  VkResult CreateTexture(const std::string& filename, VkDeviceMemory* pOutImageMemory, VkImage* pOutImage,
                         VkDeviceSize* pOutImageSize);
  glm::vec4 ReadPixelFromImage(VkImage image, uint32_t width, uint32_t height, int mouseX, int mouseY);

  VkResult CreateVkBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties,
                          VkBuffer* pOutBuffer, VkDeviceMemory* pOutBufferMemory);
};

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties) {
  // Get properties of physical device memory This call will store memory parameters (number of heaps, their size, and types) of the
  // physical device used for processing.
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
    if ((allowedTypes & (1 << i))  // index of memory type must match corresponding bit in allowedTypes
        && (memProperties.memoryTypes[i].propertyFlags & properties) ==
               properties)  // Desired property bit flags are part of memory type's property flags
    {
      // This memory type is vaild, so return its index
      return i;
    }
  }
}

static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
                         VkMemoryPropertyFlags bufferProperties, VkBuffer* pOutBuffer, VkDeviceMemory* pOutBufferMemory,
                         bool deviceAddressFlag = false) {
  // CREATE VERTEX BUFFER
  // Information to create a buffer (doesn't include assigning memory)
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = bufferSize;                        // size of buffer (size of 1 vertex * number of vertices)
  bufferCreateInfo.usage = bufferUsage;                      // Multiple types of buffer possible, we want vertex buffer
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // Similar to Swapchain images, can share vertex buffers

  // In Vulkan Cookbook 166p, Buffers don't have their own memory.
  VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, pOutBuffer));

  // GET BUFFER MEMORY REQUIRMENTS
  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device, *pOutBuffer, &memRequirements);

  VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {};
  memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
  memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

  // ALLOCATE MEMORY TO BUFFER
  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.allocationSize = memRequirements.size;
  memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(
      physicalDevice, memRequirements.memoryTypeBits,  // Index of memory type on physical device that has required bit flags
      bufferProperties);                               // VK_MEMORY_.._HOST_VISIBLE_BIT : CPU can interact with memory(GPU)
  // VK_MEMORY_.._HOST_COHERENT_BIT : Allows placement of data straight into buffer after mapping (otherwise would have to specify
  // maually) Allocate Memory to VkDeviceMemory
  memAllocInfo.pNext = deviceAddressFlag ? &memoryAllocateFlagsInfo : VK_NULL_HANDLE;
  VK_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, pOutBufferMemory));

  // Bind memory to given vertex buffer
  vkBindBufferMemory(device, *pOutBuffer, *pOutBufferMemory, 0);
}

static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer,
                       VkBuffer dstBuffer, VkDeviceSize bufferSize) {
  // Create Buffer
  // Command buffer to hold transfer commands
  VkCommandBuffer transferCommandBuffer;

  // Command Buffer details
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = transferCommandPool;
  allocInfo.commandBufferCount = 1;

  // Allocate command buffer and pool
  VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer));

  // Information to begin the command buffer record
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // We're only using the command bufer once, so set up for one time submit.

  // Begine recording transfer commands
  vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

  // Region of data to copy from and to
  VkBufferCopy bufferCopyRegion = {};
  bufferCopyRegion.srcOffset = 0;
  bufferCopyRegion.dstOffset = 0;
  bufferCopyRegion.size = bufferSize;

  vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

  // End commands
  vkEndCommandBuffer(transferCommandBuffer);

  // Queue submission information
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &transferCommandBuffer;

  // Submit Transfer command to transfer queue and wait until it finishes
  vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(transferQueue);  // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

  // Free Temporary command buffer back to pool
  vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

static void CopyImage(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkImage dstImage,
                      uint32_t width, uint32_t height) {
  // Create Buffer
  // Command buffer to hold transfer commands
  VkCommandBuffer transferCommandBuffer;

  // Command Buffer details
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = transferCommandPool;
  allocInfo.commandBufferCount = 1;

  // Allocate command buffer and pool
  VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer));

  // Information to begin the command buffer record
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // We're only using the command bufer once, so set up for one time submit.

  // Begine recording transfer commands
  vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

  VkBufferImageCopy imageRegion = {};
  imageRegion.bufferOffset = 0;                                         // Offset into data
  imageRegion.bufferRowLength = 0;                                      // Row length of data to caclulate data spacing
  imageRegion.bufferImageHeight = 0;                                    // Image height to caclulate data spacing
  imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // Which aspect of image to copy
  imageRegion.imageSubresource.mipLevel = 0;                            // Mipmap level to copy
  imageRegion.imageSubresource.baseArrayLayer = 0;                      // Staring array layer (if array)
  imageRegion.imageSubresource.layerCount = 1;                          // Number of layers to copy starting at baseArrayLayer
  imageRegion.imageOffset = {0, 0, 0};                                  // Offset into image (as opposed to raw data in bufferOffset)
  imageRegion.imageExtent = {width, height, 1};                         // Size of region to copy as (x, y, z) values

  vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

  // End commands
  vkEndCommandBuffer(transferCommandBuffer);

  // Queue submission information
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &transferCommandBuffer;

  // Submit Transfer command to transfer queue and wait until it finishes
  vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(transferQueue);  // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

  // Free Temporary command buffer back to pool
  vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

static VkResult CreateImage2D(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                              VkDeviceMemory* pOutImageMemory, VkImage* pOutVkImage, VkFormat format, VkImageUsageFlags usage,
                              VkMemoryPropertyFlags propFlags, int numberOfMipLevls = 1) {
  // CREATE IMAGE
  // Image creation info
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;  // Type of Image (1D, 2D or 3D)
  imageCreateInfo.extent.width = width;
  imageCreateInfo.extent.height = height;
  imageCreateInfo.extent.depth = 1;
  imageCreateInfo.mipLevels = numberOfMipLevls;               // Number of mipmap levels
  imageCreateInfo.arrayLayers = 1;                            // Number of levels in image array
  imageCreateInfo.format = format;                            // Format type of image
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;           // How image data should be "tiled" (arranged for optimal reading)
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // Layout of image data on creation (프레임버퍼에 맞게 변형됨)
  imageCreateInfo.usage = usage;                              // Bit flags defining what image will be used for
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;            // Number of samples for multi-sampling
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;    // Whether image can be shared between queues

  VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, pOutVkImage);
  if (result != VK_SUCCESS) {
    assert(false && "Failed to Craete An Image");
  }

  // CRAETE MEMORY FOR IMAGE
  // Get memory requirements for a type of image
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, *pOutVkImage, &memRequirements);

  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.allocationSize = memRequirements.size;
  memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, propFlags);

  VK_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, pOutImageMemory));

  // Connect memory to Image
  VK_CHECK(vkBindImageMemory(device, *pOutVkImage, *pOutImageMemory, 0));

  return VK_SUCCESS;
}

static VkResult CreateImageView(VkDevice device, VkImage image, VkImageView* pOutImageView, VkFormat format,
                                VkImageAspectFlags aspectFlags, int baseMip = 0, int maxLevelCount = 1) {
  VkImageViewCreateInfo viewCreateInfo = {};
  viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCreateInfo.image = image;                                 // image to create view for
  viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;              // Type of image (1D, 2D, 3D, Cube, etc)
  viewCreateInfo.format = format;                               // Format of image data
  viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // Allows remapping of rgba components to rgba values
  viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  // Subresources allow the view to view only a part of an image
  viewCreateInfo.subresourceRange.aspectMask = aspectFlags;    // Which aspect of image to view (e.g. COLOR_BIT for viewing color)
  viewCreateInfo.subresourceRange.baseMipLevel = baseMip;      // Start Mipmap level to view from
  viewCreateInfo.subresourceRange.levelCount = maxLevelCount;  // Number of mipmap levels to view
  viewCreateInfo.subresourceRange.baseArrayLayer = 0;          // Start array level to view from
  viewCreateInfo.subresourceRange.layerCount = 1;              // Number of array levels to view

  // Create Image view and return it
  VK_CHECK(vkCreateImageView(device, &viewCreateInfo, nullptr, pOutImageView));

  return VK_SUCCESS;
}

static void TransitionImageLayout(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlag) {
  // Create Buffer
  // Command buffer to hold transfer commands
  VkCommandBuffer transferCommandBuffer;

  // Command Buffer details
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = transferCommandPool;
  allocInfo.commandBufferCount = 1;

  // Allocate command buffer and pool
  VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer));

  // Information to begin the command buffer record
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // We're only using the command bufer once, so set up for one time submit.

  // Begine recording transfer commands
  vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

  VkImageMemoryBarrier imageMemBarrier = {};
  imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemBarrier.oldLayout = oldLayout;                          // Layout to transition from
  imageMemBarrier.newLayout = newLayout;                          // Layout to transition to
  imageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Queue family to transition from
  imageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Queue family to transition to
  imageMemBarrier.image = image;                                  // image being aceesed and modified as part of barrier
  imageMemBarrier.subresourceRange.aspectMask = aspectFlag;       // Aspect of Image being altered
  imageMemBarrier.subresourceRange.baseMipLevel = 0;              // First mip level to start alterations on
  imageMemBarrier.subresourceRange.levelCount = 1;                // number of mip levels to alter starting from base mip level
  imageMemBarrier.subresourceRange.baseArrayLayer = 0;            // First layer to start alterations on
  imageMemBarrier.subresourceRange.layerCount = 1;                // number of layers to alter starting from base array layer

  VkPipelineStageFlags srcStage = {};
  VkPipelineStageFlags dstStage = {};

  // If transitioning from new image to image ready to  recevie data...
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    imageMemBarrier.srcAccessMask = 0;                             // Memory access stage transition must after...
    imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;  // Memory access stage transition must before...

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  // If transitioning from transfer dst to shader readable...
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    // Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
    imageMemBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    // Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
    imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // srcStage: 이미지가 depth/stencil attachment로 사용될 때 최종 프래그먼트 테스트 후에 접근 가능하도록 설정합니다.
    srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    // dstStage: 셰이더 단계에서 읽어야 하므로 프래그먼트 셰이더에 맞추어 설정합니다.
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    // Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
    imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
    imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // srcStage: Color attachment에 쓰는 단계인 color attachment output 단계에서 접근할 수 있도록 설정합니다.
    srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // dstStage: 프래그먼트 셰이더에서 읽기 위해 프래그먼트 셰이더 단계로 설정합니다.
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  vkCmdPipelineBarrier(transferCommandBuffer, srcStage, dstStage,  // Pipeline Stages (match to src and dst AccessMasks)
                       0,                                          // Dependency flags
                       0, nullptr,                                 // Memory Barreir count + data
                       0, nullptr,                                 // Buffer Memory Barrier count + data
                       1, &imageMemBarrier                         // Image Memory Barrier count + data
  );

  // End commands
  vkEndCommandBuffer(transferCommandBuffer);

  // Queue submission information
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &transferCommandBuffer;

  // Submit Transfer command to transfer queue and wait until it finishes
  vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(transferQueue);  // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

  // Free Temporary command buffer back to pool
  vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

static void CmdImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkImageAspectFlags aspectFlag) {
  VkImageMemoryBarrier imageMemBarrier = {};
  imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemBarrier.oldLayout = oldLayout;                          // Layout to transition from
  imageMemBarrier.newLayout = newLayout;                          // Layout to transition to
  imageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Queue family to transition from
  imageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Queue family to transition to
  imageMemBarrier.image = image;                                  // image being aceesed and modified as part of barrier
  imageMemBarrier.subresourceRange.aspectMask = aspectFlag;       // Aspect of Image being altered
  imageMemBarrier.subresourceRange.baseMipLevel = 0;              // First mip level to start alterations on
  imageMemBarrier.subresourceRange.levelCount = 1;                // number of mip levels to alter starting from base mip level
  imageMemBarrier.subresourceRange.baseArrayLayer = 0;            // First layer to start alterations on
  imageMemBarrier.subresourceRange.layerCount = 1;                // number of layers to alter starting from base array layer

  VkPipelineStageFlags srcStage = {};
  VkPipelineStageFlags dstStage = {};

  // If transitioning from new image to image ready to  recevie data...
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    imageMemBarrier.srcAccessMask = 0;                             // Memory access stage transition must after...
    imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;  // Memory access stage transition must before...

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  // If transitioning from transfer dst to shader readable...
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    // Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
    imageMemBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    // Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
    imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // srcStage: 이미지가 depth/stencil attachment로 사용될 때 최종 프래그먼트 테스트 후에 접근 가능하도록 설정합니다.
    srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    // dstStage: 셰이더 단계에서 읽어야 하므로 프래그먼트 셰이더에 맞추어 설정합니다.
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || oldLayout == VK_IMAGE_LAYOUT_GENERAL) {
    // Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
    imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // srcStage: Color attachment에 쓰는 단계인 color attachment output 단계에서 접근할 수 있도록 설정합니다.
    srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // dstStage: 프래그먼트 셰이더에서 읽기 위해 프래그먼트 셰이더 단계로 설정합니다.
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage,  // Pipeline Stages (match to src and dst AccessMasks)
                       0,                                  // Dependency flags
                       0, nullptr,                         // Memory Barreir count + data
                       0, nullptr,                         // Buffer Memory Barrier count + data
                       1, &imageMemBarrier                 // Image Memory Barrier count + data
  );
}

static void CmdImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkImageAspectFlags aspectFlag, VkAccessFlags srcAccessFlag, VkAccessFlags dstAccessFlag,
                            VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
  VkImageMemoryBarrier imageMemBarrier = {};
  imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemBarrier.oldLayout = oldLayout;                          // Layout to transition from
  imageMemBarrier.newLayout = newLayout;                          // Layout to transition to
  imageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Queue family to transition from
  imageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Queue family to transition to
  imageMemBarrier.image = image;                                  // image being aceesed and modified as part of barrier
  imageMemBarrier.subresourceRange.aspectMask = aspectFlag;       // Aspect of Image being altered
  imageMemBarrier.subresourceRange.baseMipLevel = 0;              // First mip level to start alterations on
  imageMemBarrier.subresourceRange.levelCount = 1;                // number of mip levels to alter starting from base mip level
  imageMemBarrier.subresourceRange.baseArrayLayer = 0;            // First layer to start alterations on
  imageMemBarrier.subresourceRange.layerCount = 1;                // number of layers to alter starting from base array layer

  imageMemBarrier.srcAccessMask = srcAccessFlag;
  // Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
  imageMemBarrier.dstAccessMask = dstAccessFlag;

  vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage,  // Pipeline Stages (match to src and dst AccessMasks)
                       0,                                  // Dependency flags
                       0, nullptr,                         // Memory Barreir count + data
                       0, nullptr,                         // Buffer Memory Barrier count + data
                       1, &imageMemBarrier                 // Image Memory Barrier count + data
  );
}

static void CreateSampler(VkDevice device, VkSamplerAddressMode addressMode, VkFilter filter, VkSampler* pOutSampler,
                          bool isAnisotropy = false) {
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = filter;                                    // Magnification filter
  samplerCreateInfo.minFilter = filter;                                    // Minification filter
  samplerCreateInfo.addressModeU = addressMode;                            // Wrapping mode for U
  samplerCreateInfo.addressModeV = addressMode;                            // Wrapping mode for V
  samplerCreateInfo.addressModeW = addressMode;                            // Wrapping mode for W
  samplerCreateInfo.anisotropyEnable = isAnisotropy ? VK_TRUE : VK_FALSE;  // Disable anisotropic filtering
  samplerCreateInfo.maxAnisotropy = 1.0f;                                  // Maximum anisotropy
  samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;        // Border color
  samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;                    // Use normalized texture coordinates
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = 8.0f;

  VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, pOutSampler));
}

}  // namespace VkUtils

#define g_ResourceManager VkUtils::ResourceManager::Get()