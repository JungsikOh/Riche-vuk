#include "GfxResourceManager.h"
#include "GfxCommandPool.h"

void GfxResourceManager::CreateFence()
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkResult result = vkCreateFence(m_pDevice->logicalDevice, &fenceCreateInfo, nullptr, &m_pFence);
	if (result)
	{
		assert(false && "Failed to Create Fence in ResourceManager");
	}
}

void GfxResourceManager::CleanupFence()
{
	vkDestroyFence(m_pDevice->logicalDevice, m_pFence, nullptr);
}

void GfxResourceManager::CreateCommandPool()
{
	// Create Command Pool
	m_pCommandPool = new GfxCommandPool;
	m_pCommandPool->Initialize(m_pDevice->logicalDevice, m_pDevice->trasferQueueFamilyIndex);
}

void GfxResourceManager::CleanupCommandPool()
{
	if (m_pCommandPool)
	{
		m_pCommandPool->Free(m_pCommandBuffer);
		m_pCommandPool->Destroy();
	}
}

void GfxResourceManager::WaitForFenceValue()
{
	// Wait for given fence to signal (open) from last draw before continuing
}

void GfxResourceManager::Cleanup()
{
	CleanupCommandPool();
	CleanupFence();
}

GfxResourceManager::GfxResourceManager()
{
}

GfxResourceManager::~GfxResourceManager()
{
	Cleanup();
}

void GfxResourceManager::Initialize(GfxDevice* pDevice)
{
	m_pDevice = pDevice;
	CreateCommandPool();
}

VkResult GfxResourceManager::CreateVertexBuffer(uint32_t sizePerVertex, uint32_t vertexNum, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer, void* pInitData)
{
	// Get size of buffer needed for vertices
	VkDeviceSize bufferSize = sizePerVertex * vertexNum;

	VkBuffer pVertexBuffer;
	VkDeviceMemory pVertexBufferMemory;

	// Temporaray buffer to "stage" vertex data befroe transferring to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// Create Buffer, Allocate Memory and Bind to it
	CreateBuffer(m_pDevice->logicalDevice, m_pDevice->physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	// MAP MEMORY TO VERTEX BUFFER
	void* pData = nullptr;																				// 1. Create pointer to a point in normal memory
	vkMapMemory(m_pDevice->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &pData);				// 2. "Map" the vertex buffer memory to that point
	memcpy(pData, pInitData, (size_t)bufferSize);														// 3. Copy memory from vertices vector to that point
	vkUnmapMemory(m_pDevice->logicalDevice, stagingBufferMemory);															// 4. "UnMap" the vertex buffer memory

	// Create Buffer win TRANSFER_DST_BIT to mark as recipient of transfer data
	// Buffer Memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU (host) 
	CreateBuffer(m_pDevice->logicalDevice, m_pDevice->physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pVertexBuffer, &pVertexBufferMemory);

	// Copy staging buffer to vertex buffer on GPU
	CopyBuffer(m_pDevice->logicalDevice, m_pDevice->transferQueue, m_pCommandPool->GetVkCommandPool(), stagingBuffer, pVertexBuffer, bufferSize);

	*pOutBuffer = pVertexBuffer;
	*pOutVertexBufferMemory = pVertexBufferMemory;

	// Clean up staging buffer
	vkDestroyBuffer(m_pDevice->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_pDevice->logicalDevice, stagingBufferMemory, nullptr);

	return VK_SUCCESS;
}

VkResult GfxResourceManager::CreateIndexBuffer(uint32_t indexNum, VkDeviceMemory* pOutIndexBufferMemory, VkBuffer* pOutBuffer, void* pInitData)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indexNum;

	VkBuffer pIndexBuffer;
	VkDeviceMemory pIndexBufferMemory;

	// Temporaray buffer to "stage" vertex data befroe transferring to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// Create Buffer, Allocate Memory and Bind to it
	CreateBuffer(m_pDevice->logicalDevice, m_pDevice->physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	// MAP MEMORY TO VERTEX BUFFER
	void* pData = nullptr;														// 1. Create pointer to a point in normal memory
	vkMapMemory(m_pDevice->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &pData);			// 2. "Map" the vertex buffer memory to that point
	memcpy(pData, pInitData, (size_t)bufferSize);							// 3. Copy memory from vertices vector to that point
	vkUnmapMemory(m_pDevice->logicalDevice, stagingBufferMemory);									// 4. "UnMap" the vertex buffer memory

	CreateBuffer(m_pDevice->logicalDevice, m_pDevice->physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pIndexBuffer, &pIndexBufferMemory);

	// Copy staging buffer to vertex buffer on GPU
	CopyBuffer(m_pDevice->logicalDevice, m_pDevice->transferQueue, m_pCommandPool->GetVkCommandPool(), stagingBuffer, pIndexBuffer, bufferSize);

	*pOutBuffer = pIndexBuffer;
	*pOutIndexBufferMemory = pIndexBufferMemory;

	// Clean up staging buffer
	vkDestroyBuffer(m_pDevice->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_pDevice->logicalDevice, stagingBufferMemory, nullptr);

	return VK_SUCCESS;
}

VkResult GfxResourceManager::CreateImage(uint32_t width, uint32_t height, VkImage* pOutVkImage, VkFormat format, void* pInitImage)
{
	return VK_SUCCESS;
}
