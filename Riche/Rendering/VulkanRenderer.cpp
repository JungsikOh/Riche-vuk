#include "VulkanRenderer.h"

BOOL __stdcall VulkanRenderer::Initialize(GLFWwindow* window)
{
	m_pDevice = std::make_shared<GfxDevice>();
	m_pDevice->Initialize(window);

	// TODO : ResourceManager, DescriptorManger Init
	m_pResourceManager = std::make_shared<GfxResourceManager>();
	m_pResourceManager->Initialize(m_pDevice.get());

	return true;
}
