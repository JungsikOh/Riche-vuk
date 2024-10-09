#include "VulkanRenderer.h"

BOOL __stdcall VulkanRenderer::Initialize(GLFWwindow* window)
{
	m_pDevice = std::make_shared<GfxDevice>();
	m_pDevice->Initialize(window);

	// TODO : ResourceManager, DescriptorManger Init

	return true;
}
