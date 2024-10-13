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

VkResult GfxResourceManager::CreateImage2D(
	uint32_t width, uint32_t height, 
	VkDeviceMemory* pOutImageMemory, VkImage* pOutVkImage, VkFormat format,
	VkImageUsageFlags usage, VkMemoryPropertyFlags propFlags, void* pInitImage)
{
	// CREATE IMAGE
	// Image creation info
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;					// Type of Image (1D, 2D or 3D)
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;									// Number of mipmap levels
	imageCreateInfo.arrayLayers = 1;								// Number of levels in image array
	imageCreateInfo.format = format;								// Format type of image
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;								// How image data should be "tiled" (arranged for optimal reading)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// Layout of image data on creation (프레임버퍼에 맞게 변형됨)
	imageCreateInfo.usage = usage;								// Bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;				// Number of samples for multi-sampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Whether image can be shared between queues

	VkResult result = vkCreateImage(m_pDevice->logicalDevice, &imageCreateInfo, nullptr, pOutVkImage);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Craete An Image");
	}

	// CRAETE MEMORY FOR IMAGE
	// Get memory requirements for a type of image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_pDevice->logicalDevice, *pOutVkImage, &memRequirements);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(m_pDevice->physicalDevice, memRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(m_pDevice->logicalDevice, &memAllocInfo, nullptr, pOutImageMemory);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to Allocate An Image memory");
	}

	// Connect memory to Image
	vkBindImageMemory(m_pDevice->logicalDevice, *pOutVkImage, *pOutImageMemory, 0);

	return VK_SUCCESS;
}

VkResult GfxResourceManager::CreateImageView(VkImage image, VkImageView* pOutImageView, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;											// image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (1D, 2D, 3D, Cube, etc)
	viewCreateInfo.format = format;											// Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping of rgba components to rgba values
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;				// Which aspect of image to view (e.g. COLOR_BIT for viewing color)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;						// Start Mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;							// Number of mipmap levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;						// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;							// Number of array levels to view

	// Create Image view and return it
	VkResult result = vkCreateImageView(m_pDevice->logicalDevice, &viewCreateInfo, nullptr, pOutImageView);
	if (result != VK_SUCCESS)
	{
		assert(false && "Failed to create an Image View!");
	}

	return VK_SUCCESS;
}
