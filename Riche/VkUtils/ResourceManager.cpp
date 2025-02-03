#include "ResourceManager.h"

namespace VkUtils {
void ResourceManager::CreateFence() {}

void ResourceManager::CleanupFence() {}

void ResourceManager::CreateCommandPool() {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = m_queueFamilyIndices.transferFamily;  // Queue family type that buffers from this command pool will use

  // Create a Graphics Queue family Command Pool
  VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_TransferCommandPool));
}

void ResourceManager::CleanupCommandPool() { vkDestroyCommandPool(m_Device, m_TransferCommandPool, nullptr); }

void ResourceManager::WaitForFenceValue() {
  // Wait for given fence to signal (open) from last draw before continuing
}

void ResourceManager::Cleanup() {
  CleanupCommandPool();
  CleanupFence();
}

ResourceManager::ResourceManager() {}

ResourceManager::~ResourceManager() {}

void ResourceManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, QueueFamilyIndices indices) {
  m_Device = device;
  m_PhysicalDevice = physicalDevice;
  m_transferQueue = queue;
  m_queueFamilyIndices = indices;
  CreateCommandPool();
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
  CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  // MAP MEMORY TO VERTEX BUFFER
  void* pData = nullptr;                                                 // 1. Create pointer to a point in normal memory
  vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &pData);  // 2. "Map" the vertex buffer memory to that point
  memcpy(pData, pInitData, (size_t)bufferSize);                          // 3. Copy memory from vertices vector to that point
  vkUnmapMemory(m_Device, stagingBufferMemory);                          // 4. "UnMap" the vertex buffer memory

  // Create Buffer win TRANSFER_DST_BIT to mark as recipient of transfer data
  // Buffer Memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU (host)
  CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pVertexBuffer, &pVertexBufferMemory);

  // Copy staging buffer to vertex buffer on GPU
  CopyBuffer(m_Device, m_transferQueue, m_TransferCommandPool, stagingBuffer, pVertexBuffer, bufferSize);

  *pOutBuffer = pVertexBuffer;
  *pOutVertexBufferMemory = pVertexBufferMemory;

  // Clean up staging buffer
  vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
  vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

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
  CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  // MAP MEMORY TO VERTEX BUFFER
  void* pData = nullptr;                                                 // 1. Create pointer to a point in normal memory
  vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &pData);  // 2. "Map" the vertex buffer memory to that point
  memcpy(pData, pInitData, (size_t)bufferSize);                          // 3. Copy memory from vertices vector to that point
  vkUnmapMemory(m_Device, stagingBufferMemory);                          // 4. "UnMap" the vertex buffer memory

  // Create Buffer win TRANSFER_DST_BIT to mark as recipient of transfer data
  // Buffer Memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU (host)
  CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pVertexBuffer, &pVertexBufferMemory);

  // Copy staging buffer to vertex buffer on GPU
  CopyBuffer(m_Device, m_transferQueue, m_TransferCommandPool, stagingBuffer, pVertexBuffer, bufferSize);

  *pOutBuffer = pVertexBuffer;
  *pOutVertexBufferMemory = pVertexBufferMemory;

  // Clean up staging buffer
  vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
  vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

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
  CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  // MAP MEMORY TO VERTEX BUFFER
  void* pData = nullptr;                                                 // 1. Create pointer to a point in normal memory
  vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &pData);  // 2. "Map" the vertex buffer memory to that point
  memcpy(pData, pInitData, (size_t)bufferSize);                          // 3. Copy memory from vertices vector to that point
  vkUnmapMemory(m_Device, stagingBufferMemory);                          // 4. "UnMap" the vertex buffer memory

  CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pIndexBuffer, &pIndexBufferMemory);

  // Copy staging buffer to vertex buffer on GPU
  CopyBuffer(m_Device, m_transferQueue, m_TransferCommandPool, stagingBuffer, pIndexBuffer, bufferSize);

  *pOutBuffer = pIndexBuffer;
  *pOutIndexBufferMemory = pIndexBufferMemory;

  // Clean up staging buffer
  vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
  vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

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
  CreateBuffer(m_Device, m_PhysicalDevice, *pOutImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer,
               &imageStagingBufferMemory);

  // Copy image data to staging buffer
  void* pData;
  vkMapMemory(m_Device, imageStagingBufferMemory, 0, *pOutImageSize, 0, &pData);
  memcpy(pData, imageData, static_cast<size_t>(*pOutImageSize));
  vkUnmapMemory(m_Device, imageStagingBufferMemory);

  // Free Original image data
  stbi_image_free(imageData);

  // Create Image to hold final texture
  VkUtils::CreateImage2D(m_Device, m_PhysicalDevice, width, height, pOutImageMemory, pOutImage, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Transition image to be DST for copy operation
  TransitionImageLayout(m_Device, m_transferQueue, m_TransferCommandPool, *pOutImage, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
  // Copy Image data
  CopyImage(m_Device, m_transferQueue, m_TransferCommandPool, imageStagingBuffer, *pOutImage, width, height);
  // Transition image to be shader readable for shader usage
  TransitionImageLayout(m_Device, m_transferQueue, m_TransferCommandPool, *pOutImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

  // Destroy Staging buffer
  vkDestroyBuffer(m_Device, imageStagingBuffer, nullptr);
  vkFreeMemory(m_Device, imageStagingBufferMemory, nullptr);

  return VK_SUCCESS;
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
  VK_CHECK(vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, pOutBuffer));

  // GET BUFFER MEMORY REQUIRMENTS
  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_Device, *pOutBuffer, &memRequirements);

  // ALLOCATE MEMORY TO BUFFER
  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.allocationSize = memRequirements.size;
  memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(
      m_PhysicalDevice, memRequirements.memoryTypeBits,  // Index of memory type on physical device that has required bit flags
      bufferProperties);                                 // VK_MEMORY_.._HOST_VISIBLE_BIT : CPU can interact with memory(GPU)
  // VK_MEMORY_.._HOST_COHERENT_BIT : Allows placement of data straight into buffer after mapping (otherwise would have to specify
  // maually) Allocate Memory to VkDeviceMemory
  VK_CHECK(vkAllocateMemory(m_Device, &memAllocInfo, nullptr, pOutBufferMemory));

  // Bind memory to given vertex buffer
  vkBindBufferMemory(m_Device, *pOutBuffer, *pOutBufferMemory, 0);

  return VK_SUCCESS;
}
}  // namespace VkUtils