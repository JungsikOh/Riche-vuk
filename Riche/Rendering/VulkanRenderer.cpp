#include "VulkanRenderer.h"

BOOL __stdcall VulkanRenderer::Initialize(GLFWwindow* window)
{
	m_pDevice = std::make_shared<GfxDevice>();
	m_pDevice->Initialize(window);

	// TODO : ResourceManager, DescriptorManger Init
	m_pResourceManager = std::make_shared<GfxResourceManager>();
	m_pResourceManager->Initialize(m_pDevice.get());

	// Get swap chain images(first count, then values)
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(m_pDevice->logicalDevice, m_pDevice->swapchain.swapchain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(m_pDevice->logicalDevice, m_pDevice->swapchain.swapchain, &swapChainImageCount, images.data());

	for (VkImage image : images)
	{
		// Store the image handle
		m_pDevice->swapchain.images.push_back(image);

		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.image = image;											// image to create view for
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (1D, 2D, 3D, Cube, etc)
		viewCreateInfo.format = m_pDevice->swapchain.format;											// Format of image data
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping of rgba components to rgba values
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// Subresources allow the view to view only a part of an image
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;		// Which aspect of image to view (e.g. COLOR_BIT for viewing color)
		viewCreateInfo.subresourceRange.baseMipLevel = 0;							// Start Mipmap level to view from
		viewCreateInfo.subresourceRange.levelCount = 1;								// Number of mipmap levels to view
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;							// Start array level to view from
		viewCreateInfo.subresourceRange.layerCount = 1;								// Number of array levels to view

		// Create Image view and return it
		VkImageView imageView;
		VkResult result = vkCreateImageView(m_pDevice->logicalDevice, &viewCreateInfo, nullptr, &imageView);
		if (result != VK_SUCCESS)
		{
			assert(false && "Failed to create an Swapchain Image View!");
		}
		m_pDevice->swapchain.imageViews.push_back(imageView);
	}

	// Descriptor Allocator/LayoutCache
	m_pDescriptorAllocator = std::make_shared<GfxDescriptorAllocator>();
	m_pDescriptorAllocator->Initialize(m_pDevice->logicalDevice);
	m_pDescriptorLayoutCache = std::make_shared<GfxDescriptorLayoutCache>();
	m_pDescriptorLayoutCache->Initialize(m_pDevice->logicalDevice);

	// Command Pool/Buffer
	m_pGraphicsCommandPool = std::make_shared<GfxCommandPool>();
	m_pGraphicsCommandPool->Initialize(m_pDevice->logicalDevice, m_pDevice->graphicsQueueFamilyIndex);
	m_pCommandBuffers.resize(m_pDevice->swapchain.imageViews.size());
	for (size_t i = 0; i < m_pDevice->swapchain.imageViews.size(); ++i)
	{
		m_pCommandBuffers.push_back(std::make_shared<GfxCommandBuffer>());
		m_pCommandBuffers[m_pCommandBuffers.size() - 1]->Initialize(m_pDevice->logicalDevice, m_pGraphicsCommandPool.get());
	}

	// Buffer / Image
	for (size_t i = 0; i < m_pDevice->swapchain.imageViews.size(); ++i)
	{
		m_pSwapchainDepthStencilImages.push_back(std::make_shared<GfxImage>());
		m_pSwapchainDepthStencilImages[i]->Initialize(m_pDevice->logicalDevice, m_pDevice->physicalDevice);

		m_pResourceManager->CreateImage2D(m_pDevice->swapchain.extent.width, m_pDevice->swapchain.extent.height,
			&m_pSwapchainDepthStencilImages[i]->GetImageMemory(), &m_pSwapchainDepthStencilImages[i]->GetImage(),
			VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);
		m_pResourceManager->CreateImageView(m_pSwapchainDepthStencilImages[i]->GetImage(), &m_pSwapchainDepthStencilImages[i]->GetImageView(), VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
		
	// Render Pass
	CreateSubpass();
	CreateRenderPass();
	CreateFrameBuffer();
	CreateGraphicsPipeline();

	RenderBasicObject();

	return true;
}

void __stdcall VulkanRenderer::BeginRender()
{
}

void VulkanRenderer::RenderBasicObject()
{
	std::vector<Vertex> meshVertices =
	{
			{ { 1.0, -1.7, 0.0 },{ 1.0f, 0.0f, 0.0f }, {1.0f, 1.0f} },	// 0
			{ { 1.0, 0.4, 0.0 },{ 0.0f, 1.0f, 0.0f }, {1.0f, 0.0f} },	    // 1
			{ { -0.4, 0.4, 0.0 },{ 0.0f, 0.0f, 1.0f }, {0.0f, 0.0f} },    // 2
			{ { -0.4, -0.4, 0.0 },{ 1.0f, 1.0f, 0.0f }, {0.0f, 1.0f} },   // 3
	};

	// Index Data
	std::vector<uint32_t> meshIndices =
	{
		0, 1, 2,
		2, 3, 0
	};

	firstMesh.pVertexBuffer = std::make_shared<GfxBuffer>();
	firstMesh.pVertexBuffer->Initialize(m_pDevice->logicalDevice, m_pDevice->physicalDevice);

	firstMesh.pIndexBuffer = std::make_shared<GfxBuffer>();
	firstMesh.pIndexBuffer->Initialize(m_pDevice->logicalDevice, m_pDevice->physicalDevice);

	m_pResourceManager->CreateVertexBuffer(sizeof(Vertex), meshVertices.size(), &firstMesh.pVertexBuffer->GetBufferMemory(), &firstMesh.pVertexBuffer->GetVkBuffer(), meshVertices.data());
	m_pResourceManager->CreateIndexBuffer(meshIndices.size(), &firstMesh.pIndexBuffer->GetBufferMemory(), &firstMesh.pIndexBuffer->GetVkBuffer(), meshIndices.data());
}

void VulkanRenderer::CreateSubpass()
{
	GfxSubpass subpass;
	subpass.Initialize(GfxPipelineType::Graphics);
	subpass.AddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	subpass.SetDepthStencilAttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	m_subpasses.push_back(subpass);
}

void VulkanRenderer::CreateRenderPass()
{
	std::vector<GfxAttachmentDesc> attachmentDescs;
	// colour
	// Swapchain
	GfxAttachmentDesc swapChainColourAttachment = {};
	swapChainColourAttachment.format = m_pDevice->swapchain.format;							// format to use for attachment
	swapChainColourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;						// number of samples to write for multisampling, relative to multisampling
	swapChainColourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					// Describes what to do with attachment before rendering
	swapChainColourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				// Describes what to do with attachment after rendering
	swapChainColourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;		// Describes what to do with stencil before rendering
	swapChainColourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		// Describes what to do with stencil after rendering

	// FrameBuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations, initial -> subpass -> final
	swapChainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;				// Image data layout before render pass starts
	swapChainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;			// Image data layout after render pass (to change to)
	attachmentDescs.push_back(swapChainColourAttachment);
	// TODO : Depth Image/Image View create
	GfxAttachmentDesc desc = {};
	desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescs.push_back(desc);

	m_pRenderPass = std::make_shared<GfxRenderPass>();
	m_pRenderPass->Initialize(m_pDevice->logicalDevice, attachmentDescs, m_subpasses);
}

void VulkanRenderer::CreateFrameBuffer()
{
	for (size_t i = 0; i < m_pDevice->swapchain.imageViews.size(); ++i)
	{
		std::vector<VkImageView> attachments = { m_pDevice->swapchain.imageViews[i], m_pSwapchainDepthStencilImages[i]->GetImageView() };
		m_pSwapchainFrameBuffers.push_back(std::make_shared<GfxFrameBuffer>());
		m_pSwapchainFrameBuffers[i]->Initialize(m_pDevice->swapchain.extent.width, m_pDevice->swapchain.extent.height, m_pRenderPass.get(), attachments);
	}
}

void VulkanRenderer::CreateGraphicsPipeline()
{
	m_pGraphicsPipeline = std::make_shared<GfxPipeline>();
	m_pGraphicsPipeline->Initialize(m_pDevice->logicalDevice);

	//m_pGraphicsPipeline->SetVertexShader("../Resources/Shaders/basic_vert.spv");
	//m_pGraphicsPipeline->SetFragmentShader("../Resources/Shaders/basic_frag.spv");
	//m_pGraphicsPipeline->AddInputBindingDescription(0, static_cast<uint32_t>(sizeof(Vertex)));
}
