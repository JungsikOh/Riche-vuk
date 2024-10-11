#pragma once

#include "IRenderer.h"
#include "Components.h"
#include "Graphics/GfxCore.h"

class GfxDevice;
class GfxPipeline;
class GfxFrameBuffer;
class GfxRenderPass;
class GfxSubpass;
class GfxResourceManager;

class VulkanRenderer : public IRenderer
{
	static const UINT MAX_DRAW_COUNT_PER_FRAME = 4096;
	static const UINT MAX_DESCRIPTOR_COUNT = 4096;
	static const UINT MAX_RENDER_THREAD_COUNT = 8;
	
	// Gfx
	std::shared_ptr<GfxDevice> m_pDevice;

	std::shared_ptr<GfxCommandPool> m_pCommandPool;
	std::shared_ptr<GfxCommandBuffer> m_pCommandBuffer;

	std::shared_ptr<GfxPipeline> m_pGraphicsPipeline;

	std::vector<GfxSubpass> m_subpasses;
	std::shared_ptr<GfxRenderPass> m_pRenderPass;
	std::shared_ptr<GfxFrameBuffer> m_pFrameBuffer; // Swapchain framebuffer

	std::shared_ptr<GfxResourceManager> m_pResourceManager;
	
	// Descriptor
	std::shared_ptr<GfxDescriptorLayoutCache> m_pDescriptorLayoutCache;
	std::shared_ptr<GfxDescriptorAllocator> m_pDescriptorAllocator;
	GfxDescriptorBuilder m_descriptorBuilder;

	Mesh firstMesh = {};


public:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;

	// Derived from IRenderer
	virtual BOOL	__stdcall Initialize(GLFWwindow* window);
	virtual void	__stdcall BeginRender();
	//virtual void	__stdcall EndRender() = 0;
	//virtual void	__stdcall Present() = 0;
	///*virtual BOOL	__stdcall UpdateWindowSize(DWORD dwBackBufferWidth, DWORD dwBackBufferHeight) = 0;*/

	//virtual void* __stdcall CreateTiledTexture(UINT TexWidth, UINT TexHeight, DWORD r, DWORD g, DWORD b) = 0;
	//virtual void* __stdcall CreateDynamicTexture(UINT TexWidth, UINT TexHeight) = 0;
	//virtual void* __stdcall CreateTextureFromFile(const WCHAR* wchFileName) = 0;
	//virtual void	__stdcall DeleteTexture(void* pTexHandle) = 0;

	//virtual void	__stdcall UpdateTextureWithImage(void* pTexHandle, const BYTE* pSrcBits, UINT SrcWidth, UINT SrcHeight) = 0;

	//virtual void	__stdcall SetCameraPos(float x, float y, float z) = 0;
	//virtual void	__stdcall MoveCamera(float x, float y, float z) = 0;
	//virtual void	__stdcall GetCameraPos(float* pfOutX, float* pfOutY, float* pfOutZ) = 0;
	//virtual void	__stdcall SetCamera(const glm::vec4 pCamPos, const glm::vec4 pCamDir, const glm::vec4 pCamUp) = 0;
	//virtual DWORD	__stdcall GetCommandListCount() = 0;

private:
	void RenderBasicObject();

	void CreateSubpass();
	void CreateRenderPass();
	void CreateFrameBuffer();
	void CreateGraphicsPipeline();
};

