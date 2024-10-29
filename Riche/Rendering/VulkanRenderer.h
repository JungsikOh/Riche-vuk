#pragma once

#include "IRenderer.h"
#include "Components.h"
#include "Graphics/GfxCore.h"
#include "DescriptorManager.h"

class DescriptorManager;

class GfxDevice;
class GfxPipeline;
class GfxFrameBuffer;
class GfxRenderPass;
class GfxSubpass;
class GfxResourceManager;
class GfxImage;

class VulkanRenderer : public IRenderer
{
	static const UINT MAX_DRAW_COUNT_PER_FRAME = 4096;
	static const UINT MAX_DESCRIPTOR_COUNT = 4096;
	static const UINT MAX_RENDER_THREAD_COUNT = 8;
	
	// Gfx
	std::shared_ptr<GfxDevice> m_pDevice;

	std::shared_ptr<GfxCommandPool> m_pGraphicsCommandPool;
	std::vector<std::shared_ptr<GfxCommandBuffer>> m_pCommandBuffers;

	// Swapchain
	std::vector<std::shared_ptr<GfxImage>> m_pSwapchainDepthStencilImages;
	std::vector<std::shared_ptr<GfxFrameBuffer>> m_pSwapchainFrameBuffers;

	std::shared_ptr<GfxPipeline> m_pGraphicsPipeline;

	std::vector<GfxSubpass> m_subpasses;
	std::shared_ptr<GfxRenderPass> m_pRenderPass;

	std::shared_ptr<GfxResourceManager> m_pResourceManager;
	
	// Descriptor
	std::shared_ptr<GfxDescriptorLayoutCache> m_pDescriptorLayoutCache;
	std::shared_ptr<GfxDescriptorAllocator> m_pDescriptorAllocator;

	std::vector<GfxDescriptorBuilder> m_inputDescriptorBuilder;
	GfxDescriptorBuilder m_uboDescriptorBuilder;

	DescriptorManager m_descriptorManager;

	Mesh firstMesh = {};
	std::shared_ptr<GfxBuffer> m_pUboViewProjBuffer;

	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;


public:
	VulkanRenderer();
	~VulkanRenderer();

	// Derived from IRenderer
	virtual BOOL	__stdcall Initialize(GLFWwindow* window);
	virtual void	__stdcall BeginRender();
	//virtual void	__stdcall EndRender() = 0;
	//virtual void	__stdcall Present() = 0;
	///*virtual BOOL	__stdcall UpdateWindowSize(DWORD dwBackBufferWidth, DWORD dwBackBufferHeight) = 0;*/

	void FillCommandBuffer();

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

static std::wstring ToWideString(std::string const& in) {
	std::wstring out{};
	out.reserve(in.length());
	const char* ptr = in.data();
	const char* const end = in.data() + in.length();

	mbstate_t state{};
	wchar_t wc;
	while (size_t len = mbrtowc(&wc, ptr, end - ptr, &state)) {
		if (len == static_cast<size_t>(-1)) // bad encoding
			return std::wstring{};
		if (len == static_cast<size_t>(-2))
			break;
		out.push_back(wc);
		ptr += len;
	}
	return out;
}