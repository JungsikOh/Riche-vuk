#include "ResourceManager.h"

namespace VkUtils {
void ResourceManager::CreateFence() {
  // Create Fence for synchronization
  VkFenceCreateInfo fenceCreateInfo{};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  VK_CHECK(vkCreateFence(m_pDevice, &fenceCreateInfo, nullptr, &m_fence));
}

void ResourceManager::CleanupFence() { vkDestroyFence(m_pDevice, m_fence, nullptr); }

void ResourceManager::CreateCommandPool() {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = m_queueFamilyIndices.transferFamily;  // Queue family type that buffers from this command pool will use

  // Create a Graphics Queue family Command Pool
  VK_CHECK(vkCreateCommandPool(m_pDevice, &poolInfo, nullptr, &m_transferCommandPool));

  poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = m_queueFamilyIndices.computeFamily;  // Queue family type that buffers from this command pool will use

  // Create a Graphics Queue family Command Pool
  VK_CHECK(vkCreateCommandPool(m_pDevice, &poolInfo, nullptr, &m_commputeCommandPool));
}

void ResourceManager::CleanupCommandPool() {
  vkDestroyCommandPool(m_pDevice, m_transferCommandPool, nullptr);
  vkDestroyCommandPool(m_pDevice, m_commputeCommandPool, nullptr);
}

void ResourceManager::WaitForFenceValue() {
  // Wait for given fence to signal (open) from last draw before continuing
}

void ResourceManager::Cleanup() {
  CleanupCommandPool();
  CleanupFence();
}

void ResourceManager::CreateCommandBuffer() {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_transferCommandPool;
  allocInfo.commandBufferCount = 1;

  // Allocate command buffer and pool
  VK_CHECK(vkAllocateCommandBuffers(m_pDevice, &allocInfo, &m_transferCommandBuffer));
}

VkCommandBuffer ResourceManager::CreateAndBeginCommandBuffer() {
  VkCommandBuffer transferCommandBuffer;

  // Command Buffer details
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_transferCommandPool;
  allocInfo.commandBufferCount = 1;

  // Allocate command buffer and pool
  VK_CHECK(vkAllocateCommandBuffers(m_pDevice, &allocInfo, &transferCommandBuffer));

  // Information to begin the command buffer record
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // We're only using the command bufer once, so set up for one time submit.

  // Begine recording transfer commands
  vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

  return transferCommandBuffer;
}

void ResourceManager::EndAndSummitCommandBuffer(VkCommandBuffer commandbuffer) {
  // End commands
  vkEndCommandBuffer(commandbuffer);

  // Queue submission information
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandbuffer;

  // Create Fence for synchronization
  VkFence fence;
  VkFenceCreateInfo fenceCreateInfo{};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VK_CHECK(vkCreateFence(m_pDevice, &fenceCreateInfo, nullptr, &fence));

  // Submit Transfer command to transfer queue and wait until it finishes
  vkQueueSubmit(m_transferQueue, 1, &submitInfo, fence);

  vkWaitForFences(m_pDevice, 1, &fence, VK_TRUE, UINT64_MAX);

  vkQueueWaitIdle(m_transferQueue);  // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.
  vkDestroyFence(m_pDevice, fence, nullptr);

  // Free Temporary command buffer back to pool
  vkFreeCommandBuffers(m_pDevice, m_transferCommandPool, 1, &commandbuffer);

  // vkEndCommandBuffer(commandbuffer);

  //// Queue submission information
  // VkSubmitInfo submitInfo{};
  // submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  // submitInfo.commandBufferCount = 1;
  // submitInfo.pCommandBuffers = &commandbuffer;
  //// Submit command buffer and use fence instead of queue wait idle
  // vkQueueSubmit(m_transferQueue, 1, &submitInfo, m_fence);
  // vkResetFences(m_pDevice, 1, &m_fence);
  //
  // vkWaitForFences(m_pDevice, 1, &m_fence, VK_TRUE, UINT64_MAX);

  // Reset Command Buffer instead of freeing
  // vkResetCommandBuffer(commandbuffer, 0);
}

ResourceManager::ResourceManager() {}

ResourceManager::~ResourceManager() {}

void ResourceManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkQueue compute,
                                 QueueFamilyIndices indices) {
  m_pDevice = device;
  m_pPhysicalDevice = physicalDevice;
  m_transferQueue = queue;
  m_computeQueue = compute;
  m_queueFamilyIndices = indices;
  CreateCommandPool();
  CreateFence();
}

VkResult ResourceManager::CreateVertexBuffer(uint32_t sizePerVertex, uint32_t vertexNum, VkDeviceMemory* pOutVertexBufferMemory,
                                             VkBuffer* pOutBuffer, void* pInitData) {
  // Get size of buffer needed for vertices
  VkDeviceSize bufferSize = sizePerVertex * vertexNum;

  VkBuffer pVertexBuffer;
  VkDeviceMemory pVertexBufferMemory;

  // Temporaray buffer to "stage" vertex data befroe transferring to GPU
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  // Create Buffer, Allocate Memory and Bind to it
  CreateBuffer(m_pDevice, m_pPhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  // MAP MEMORY TO VERTEX BUFFER
  void* pData = nullptr;                                                 // 1. Create pointer to a point in normal memory
  vkMapMemory(m_pDevice, stagingBufferMemory, 0, bufferSize, 0, &pData);  // 2. "Map" the vertex buffer memory to that point
  memcpy(pData, pInitData, (size_t)bufferSize);                          // 3. Copy memory from vertices vector to that point
  vkUnmapMemory(m_pDevice, stagingBufferMemory);                          // 4. "UnMap" the vertex buffer memory

  // Create Buffer win TRANSFER_DST_BIT to mark as recipient of transfer data
  // Buffer Memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU (host)
  CreateBuffer(m_pDevice, m_pPhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pVertexBuffer, &pVertexBufferMemory);

  // Copy staging buffer to vertex buffer on GPU
  CopyBuffer(m_pDevice, m_transferQueue, m_transferCommandPool, stagingBuffer, pVertexBuffer, bufferSize);

  *pOutBuffer = pVertexBuffer;
  *pOutVertexBufferMemory = pVertexBufferMemory;

  // Clean up staging buffer
  vkDestroyBuffer(m_pDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_pDevice, stagingBufferMemory, nullptr);

  return VK_SUCCESS;
}

VkResult ResourceManager::CreateVertexBuffer(uint32_t vertexDataSize, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer,
                                             void* pInitData) {
  // Get size of buffer needed for vertices
  VkDeviceSize bufferSize = vertexDataSize;

  VkBuffer pVertexBuffer;
  VkDeviceMemory pVertexBufferMemory;

  // Temporaray buffer to "stage" vertex data befroe transferring to GPU
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  // Create Buffer, Allocate Memory and Bind to it
  CreateBuffer(m_pDevice, m_pPhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  // MAP MEMORY TO VERTEX BUFFER
  void* pData = nullptr;                                                 // 1. Create pointer to a point in normal memory
  vkMapMemory(m_pDevice, stagingBufferMemory, 0, bufferSize, 0, &pData);  // 2. "Map" the vertex buffer memory to that point
  memcpy(pData, pInitData, (size_t)bufferSize);                          // 3. Copy memory from vertices vector to that point
  vkUnmapMemory(m_pDevice, stagingBufferMemory);                          // 4. "UnMap" the vertex buffer memory

  // Create Buffer win TRANSFER_DST_BIT to mark as recipient of transfer data
  // Buffer Memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU (host)
  CreateBuffer(m_pDevice, m_pPhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pVertexBuffer, &pVertexBufferMemory);

  // Copy staging buffer to vertex buffer on GPU
  CopyBuffer(m_pDevice, m_transferQueue, m_transferCommandPool, stagingBuffer, pVertexBuffer, bufferSize);

  *pOutBuffer = pVertexBuffer;
  *pOutVertexBufferMemory = pVertexBufferMemory;

  // Clean up staging buffer
  vkDestroyBuffer(m_pDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_pDevice, stagingBufferMemory, nullptr);

  return VK_SUCCESS;
}

VkResult ResourceManager::CreateIndexBuffer(uint32_t indexDataSize, VkDeviceMemory* pOutIndexBufferMemory, VkBuffer* pOutBuffer,
                                            void* pInitData) {
  VkDeviceSize bufferSize = indexDataSize;

  VkBuffer pIndexBuffer;
  VkDeviceMemory pIndexBufferMemory;

  // Temporaray buffer to "stage" vertex data befroe transferring to GPU
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  // Create Buffer, Allocate Memory and Bind to it
  CreateBuffer(m_pDevice, m_pPhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  // MAP MEMORY TO VERTEX BUFFER
  void* pData = nullptr;                                                 // 1. Create pointer to a point in normal memory
  vkMapMemory(m_pDevice, stagingBufferMemory, 0, bufferSize, 0, &pData);  // 2. "Map" the vertex buffer memory to that point
  memcpy(pData, pInitData, (size_t)bufferSize);                          // 3. Copy memory from vertices vector to that point
  vkUnmapMemory(m_pDevice, stagingBufferMemory);                          // 4. "UnMap" the vertex buffer memory

  CreateBuffer(m_pDevice, m_pPhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pIndexBuffer, &pIndexBufferMemory);

  // Copy staging buffer to vertex buffer on GPU
  CopyBuffer(m_pDevice, m_transferQueue, m_transferCommandPool, stagingBuffer, pIndexBuffer, bufferSize);

  *pOutBuffer = pIndexBuffer;
  *pOutIndexBufferMemory = pIndexBufferMemory;

  // Clean up staging buffer
  vkDestroyBuffer(m_pDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_pDevice, stagingBufferMemory, nullptr);

  return VK_SUCCESS;
}

VkResult ResourceManager::CreateTexture(const std::string& filename, VkDeviceMemory* pOutImageMemory, VkImage* pOutImage,
                                        VkDeviceSize* pOutImageSize) {
  // Load Image file
  int width, height;
  stbi_uc* imageData = LoadTextureFile(filename, &width, &height, pOutImageSize);

  // Create Staging Buffer to hold loaded data, ready to copy to device
  VkBuffer imageStagingBuffer;
  VkDeviceMemory imageStagingBufferMemory;
  CreateBuffer(m_pDevice, m_pPhysicalDevice, *pOutImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer,
               &imageStagingBufferMemory);

  // Copy image data to staging buffer
  void* pData;
  vkMapMemory(m_pDevice, imageStagingBufferMemory, 0, *pOutImageSize, 0, &pData);
  memcpy(pData, imageData, static_cast<size_t>(*pOutImageSize));
  vkUnmapMemory(m_pDevice, imageStagingBufferMemory);

  // Free Original image data
  stbi_image_free(imageData);

  // Create Image to hold final texture
  VkUtils::CreateImage2D(m_pDevice, m_pPhysicalDevice, width, height, pOutImageMemory, pOutImage, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Transition image to be DST for copy operation
  TransitionImageLayout(m_pDevice, m_transferQueue, m_transferCommandPool, *pOutImage, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
  // Copy Image data
  CopyImage(m_pDevice, m_transferQueue, m_transferCommandPool, imageStagingBuffer, *pOutImage, width, height);
  // Transition image to be shader readable for shader usage
  TransitionImageLayout(m_pDevice, m_transferQueue, m_transferCommandPool, *pOutImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

  // Destroy Staging buffer
  vkDestroyBuffer(m_pDevice, imageStagingBuffer, nullptr);
  vkFreeMemory(m_pDevice, imageStagingBufferMemory, nullptr);

  return VK_SUCCESS;
}

glm::vec4 ResourceManager::ReadPixelFromImage(VkImage image, uint32_t width, uint32_t height, int mouseX, int mouseY) {
  // 0) 좌표 변환 (Vulkan은 보통 원점이 하단)
  //    만약 윈도우 좌표가 상단(0,0) -> 하단(screenHeight)이라면 뒤집기
  //    아래에서는 "mouseY" 뒤집는 예시
  int flippedY = (height - 1) - mouseY;

  // 경계 체크
  if (mouseX < 0 || mouseY < 0 || mouseX >= (int)width || mouseY >= (int)height) {
    std::cerr << "[ReadPixel] Invalid mouse pos" << std::endl;
    return {-1, 0, 0, 0};  // 범위를 벗어났으니 0
  }
  flippedY = mouseY;

  // 1) Staging Buffer 만들기 (1픽셀 = 4바이트 RGBA)
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;
  VkUtils::CreateBuffer(m_pDevice, m_pPhysicalDevice,
                        4 * sizeof(float),  // 4 bytes
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &stagingBuffer, &stagingMemory);

  VkCommandBuffer transferCommandBuffer;

  // Command Buffer details
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_transferCommandPool;
  allocInfo.commandBufferCount = 1;

  // Allocate command buffer and pool
  VK_CHECK(vkAllocateCommandBuffers(m_pDevice, &allocInfo, &transferCommandBuffer));

  // Information to begin the command buffer record
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // We're only using the command bufer once, so set up for one time submit.

  // Begine recording transfer commands
  vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

  // 3) Image Layout 전환: PRESENT_SRC_KHR -> TRANSFER_SRC_OPTIMAL
  //    (pipelineBarrier 혹은 imageMemoryBarrier)
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  // src Access/Stage
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

  vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // 4) vkCmdCopyImageToBuffer (1x1 크기만 복사)
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  // 아래 rowLength, imageHeight가 0이면 "타이트"하게 복사
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {mouseX, flippedY, 0};
  region.imageExtent = {1, 1, 1};  // 1픽셀

  vkCmdCopyImageToBuffer(transferCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

  // 5) 다시 Layout 전환: TRANSFER_SRC_OPTIMAL -> PRESENT_SRC_KHR (필요시)
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.dstAccessMask = 0;

  vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // End commands
  vkEndCommandBuffer(transferCommandBuffer);

  // Queue submission information
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &transferCommandBuffer;

  // Submit Transfer command to transfer queue and wait until it finishes
  vkQueueSubmit(m_transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_transferQueue);  // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

  // Free Temporary command buffer back to pool
  vkFreeCommandBuffers(m_pDevice, m_transferCommandPool, 1, &transferCommandBuffer);

  // 7) Staging Buffer를 map 해서 픽셀 읽기
  float resultColor[4];
  void* pData;
  vkMapMemory(m_pDevice, stagingMemory, 0, 4 * sizeof(float), 0, &pData);
  std::memcpy(&resultColor, pData, 4 * sizeof(float));
  vkUnmapMemory(m_pDevice, stagingMemory);

  // 8) 스테이징 버퍼/메모리 정리
  vkDestroyBuffer(m_pDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_pDevice, stagingMemory, nullptr);

  return glm::vec4(resultColor[0], resultColor[1], resultColor[2], resultColor[3]);
}

VkResult ResourceManager::CreateVkBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
                                         VkMemoryPropertyFlags bufferProperties, VkBuffer* pOutBuffer,
                                         VkDeviceMemory* pOutBufferMemory) {
  // CREATE VERTEX BUFFER
  // Information to create a buffer (doesn't include assigning memory)
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = bufferSize;                        // size of buffer (size of 1 vertex * number of vertices)
  bufferCreateInfo.usage = bufferUsage;                      // Multiple types of buffer possible, we want vertex buffer
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // Similar to Swapchain images, can share vertex buffers

  // In Vulkan Cookbook 166p, Buffers don't have their own memory.
  VK_CHECK(vkCreateBuffer(m_pDevice, &bufferCreateInfo, nullptr, pOutBuffer));

  // GET BUFFER MEMORY REQUIRMENTS
  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_pDevice, *pOutBuffer, &memRequirements);

  // ALLOCATE MEMORY TO BUFFER
  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.allocationSize = memRequirements.size;
  memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(
      m_pPhysicalDevice, memRequirements.memoryTypeBits,  // Index of memory type on physical device that has required bit flags
      bufferProperties);                                 // VK_MEMORY_.._HOST_VISIBLE_BIT : CPU can interact with memory(GPU)
  // VK_MEMORY_.._HOST_COHERENT_BIT : Allows placement of data straight into buffer after mapping (otherwise would have to specify
  // maually) Allocate Memory to VkDeviceMemory
  VK_CHECK(vkAllocateMemory(m_pDevice, &memAllocInfo, nullptr, pOutBufferMemory));

  // Bind memory to given vertex buffer
  vkBindBufferMemory(m_pDevice, *pOutBuffer, *pOutBufferMemory, 0);

  return VK_SUCCESS;
}
}  // namespace VkUtils