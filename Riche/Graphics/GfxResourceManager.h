#pragma once

class GfxDevice;
class GfxCommandPool;
class GfxCommandBuffer;

class GfxResourceManager
{
	GfxDevice* m_pDevice = nullptr;
	GfxCommandPool* m_pCommandPool = nullptr;
	GfxCommandBuffer* m_pCommandBuffer = nullptr;

	VkFence m_pFence = VK_NULL_HANDLE;

	void CreateFence();
	void CleanupFence();
	void CreateCommandPool();
	void CleanupCommandPool();

	void WaitForFenceValue();

	void Cleanup();

public:
	GfxResourceManager();
	~GfxResourceManager();
	void Initialize(GfxDevice* pDevice);
	VkResult CreateVertexBuffer(uint32_t sizePerVertex, uint32_t vertexNum, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer, void* pInitData);
	VkResult CreateIndexBuffer(uint32_t indexNum, VkDeviceMemory* pOutIndexBufferMemory, VkBuffer* pOutBuffer, void* pInitData);
};

