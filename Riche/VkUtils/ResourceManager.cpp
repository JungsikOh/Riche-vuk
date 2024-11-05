#include "ResourceManager.h"

namespace VkUtils
{
	void ResourceManager::CreateFence()
	{
	}

	void ResourceManager::CleanupFence()
	{
	}

	void ResourceManager::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_QueueFamilyIndices.transferFamily;			// Queue family type that buffers from this command pool will use

		// Create a Graphics Queue family Command Pool
		VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_TransferCommandPool));
	}

	void ResourceManager::CleanupCommandPool()
	{
		vkDestroyCommandPool(m_Device, m_TransferCommandPool, nullptr);
	}

	void ResourceManager::WaitForFenceValue()
	{
		// Wait for given fence to signal (open) from last draw before continuing
	}

	void ResourceManager::Cleanup()
	{
		CleanupCommandPool();
		CleanupFence();
	}

	ResourceManager::ResourceManager()
	{
	}

	ResourceManager::~ResourceManager()
	{
		Cleanup();
	}

	void ResourceManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, QueueFamilyIndices indices)
	{
		m_Device = device;
		m_PhysicalDevice = physicalDevice;
		m_TransferQueue = queue;
		m_QueueFamilyIndices = indices;
		CreateCommandPool();
	}

	VkResult ResourceManager::CreateVertexBuffer(uint32_t sizePerVertex, uint32_t vertexNum, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer, void* pInitData)
	{
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
		void* pData = nullptr;																				// 1. Create pointer to a point in normal memory
		vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &pData);				// 2. "Map" the vertex buffer memory to that point
		memcpy(pData, pInitData, (size_t)bufferSize);														// 3. Copy memory from vertices vector to that point
		vkUnmapMemory(m_Device, stagingBufferMemory);															// 4. "UnMap" the vertex buffer memory

		// Create Buffer win TRANSFER_DST_BIT to mark as recipient of transfer data
		// Buffer Memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU (host) 
		CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pVertexBuffer, &pVertexBufferMemory);

		// Copy staging buffer to vertex buffer on GPU
		CopyBuffer(m_Device, m_TransferQueue, m_TransferCommandPool, stagingBuffer, pVertexBuffer, bufferSize);

		*pOutBuffer = pVertexBuffer;
		*pOutVertexBufferMemory = pVertexBufferMemory;

		// Clean up staging buffer
		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

		return VK_SUCCESS;
	}

	VkResult ResourceManager::CreateIndexBuffer(uint32_t indexNum, VkDeviceMemory* pOutIndexBufferMemory, VkBuffer* pOutBuffer, void* pInitData)
	{
		VkDeviceSize bufferSize = sizeof(uint32_t) * indexNum;

		VkBuffer pIndexBuffer;
		VkDeviceMemory pIndexBufferMemory;

		// Temporaray buffer to "stage" vertex data befroe transferring to GPU
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		// Create Buffer, Allocate Memory and Bind to it
		CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

		// MAP MEMORY TO VERTEX BUFFER
		void* pData = nullptr;														// 1. Create pointer to a point in normal memory
		vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &pData);			// 2. "Map" the vertex buffer memory to that point
		memcpy(pData, pInitData, (size_t)bufferSize);							// 3. Copy memory from vertices vector to that point
		vkUnmapMemory(m_Device, stagingBufferMemory);									// 4. "UnMap" the vertex buffer memory

		CreateBuffer(m_Device, m_PhysicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pIndexBuffer, &pIndexBufferMemory);

		// Copy staging buffer to vertex buffer on GPU
		CopyBuffer(m_Device, m_TransferQueue, m_TransferCommandPool, stagingBuffer, pIndexBuffer, bufferSize);

		*pOutBuffer = pIndexBuffer;
		*pOutIndexBufferMemory = pIndexBufferMemory;

		// Clean up staging buffer
		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

		return VK_SUCCESS;
	}
}