#pragma once
#include "QueueFamilyIndices.h"
#include "Rendering/Core.h"

namespace VkUtils
{
	// TransferQueue를 이용해서 만들어야하는 Vulkan Object에 대해서 생성하는 클래스
	class ResourceManager
	{
		VkDevice m_Device;
		VkPhysicalDevice m_PhysicalDevice;
		QueueFamilyIndices m_QueueFamilyIndices;

		VkQueue m_TransferQueue;
		VkCommandPool m_TransferCommandPool;

		VkFence m_pFence = VK_NULL_HANDLE;

		void CreateFence();
		void CleanupFence();
		void CreateCommandPool();
		void CleanupCommandPool();

		void WaitForFenceValue();

		void Cleanup();

	public:
		ResourceManager();
		~ResourceManager();

		void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, QueueFamilyIndices indices);

		VkResult CreateVertexBuffer(uint32_t sizePerVertex, uint32_t vertexNum, VkDeviceMemory* pOutVertexBufferMemory, VkBuffer* pOutBuffer, void* pInitData);
		VkResult CreateIndexBuffer(uint32_t indexNum, VkDeviceMemory* pOutIndexBufferMemory, VkBuffer* pOutBuffer, void* pInitData);
	};

	static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
	{
		// Get properties of physical device memory This call will store memory parameters (number of heaps, their size, and types) of the physical device used for processing.
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			if ((allowedTypes & (1 << i))																// index of memory type must match corresponding bit in allowedTypes
				&& (memProperties.memoryTypes[i].propertyFlags & properties) == properties)				// Desired property bit flags are part of memory type's property flags
			{
				// This memory type is vaild, so return its index
				return i;
			}
		}
	}

	static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
		VkMemoryPropertyFlags bufferProperties, VkBuffer* pOutBuffer, VkDeviceMemory* pOutBufferMemory)
	{
		// CREATE VERTEX BUFFER
		// Information to create a buffer (doesn't include assigning memory)
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = bufferSize; // size of buffer (size of 1 vertex * number of vertices)
		bufferCreateInfo.usage = bufferUsage;			// Multiple types of buffer possible, we want vertex buffer
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Similar to Swapchain images, can share vertex buffers

		// In Vulkan Cookbook 166p, Buffers don't have their own memory.
		VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, pOutBuffer));

		// GET BUFFER MEMORY REQUIRMENTS
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, *pOutBuffer, &memRequirements);

		// ALLOCATE MEMORY TO BUFFER
		VkMemoryAllocateInfo memAllocInfo = {};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = memRequirements.size;
		memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits,					// Index of memory type on physical device that has required bit flags
			bufferProperties);																								// VK_MEMORY_.._HOST_VISIBLE_BIT : CPU can interact with memory(GPU)
		// VK_MEMORY_.._HOST_COHERENT_BIT : Allows placement of data straight into buffer after mapping (otherwise would have to specify maually)
		// Allocate Memory to VkDeviceMemory
		VK_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, pOutBufferMemory));

		// Bind memory to given vertex buffer
		vkBindBufferMemory(device, *pOutBuffer, *pOutBufferMemory, 0);
	}

	static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
	{
		// Create Buffer
		// Command buffer to hold transfer commands
		VkCommandBuffer transferCommandBuffer;

		// Command Buffer details
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = transferCommandPool;
		allocInfo.commandBufferCount = 1;

		// Allocate command buffer and pool
		VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer));

		// Information to begin the command buffer record
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We're only using the command bufer once, so set up for one time submit. 

		// Begine recording transfer commands
		vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

		// Region of data to copy from and to
		VkBufferCopy bufferCopyRegion = {};
		bufferCopyRegion.srcOffset = 0;
		bufferCopyRegion.dstOffset = 0;
		bufferCopyRegion.size = bufferSize;

		vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

		// End commands
		vkEndCommandBuffer(transferCommandBuffer);

		// Queue submission information
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transferCommandBuffer;

		// Submit Transfer command to transfer queue and wait until it finishes
		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(transferQueue); // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

		// Free Temporary command buffer back to pool
		vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
	}

	static VkResult CreateImage2D(
		VkDevice device, VkPhysicalDevice physicalDevice,
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

		VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, pOutVkImage);
		if (result != VK_SUCCESS)
		{
			assert(false && "Failed to Craete An Image");
		}

		// CRAETE MEMORY FOR IMAGE
		// Get memory requirements for a type of image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, *pOutVkImage, &memRequirements);

		VkMemoryAllocateInfo memAllocInfo = {};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = memRequirements.size;
		memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, propFlags);

		VK_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, pOutImageMemory));

		// Connect memory to Image
		VK_CHECK(vkBindImageMemory(device, *pOutVkImage, *pOutImageMemory, 0));

		return VK_SUCCESS;
	}

	static VkResult CreateImageView(
		VkDevice device, 
		VkImage image, VkImageView* pOutImageView, VkFormat format, VkImageAspectFlags aspectFlags)
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
		VK_CHECK(vkCreateImageView(device, &viewCreateInfo, nullptr, pOutImageView));

		return VK_SUCCESS;
	}

	static void TransitionImageLayout(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, 
		VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlag)
	{
		// Create Buffer
		// Command buffer to hold transfer commands
		VkCommandBuffer transferCommandBuffer;

		// Command Buffer details
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = transferCommandPool;
		allocInfo.commandBufferCount = 1;

		// Allocate command buffer and pool
		VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer));

		// Information to begin the command buffer record
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We're only using the command bufer once, so set up for one time submit. 

		// Begine recording transfer commands
		vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

		VkImageMemoryBarrier imageMemBarrier = {};
		imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemBarrier.oldLayout = oldLayout;							// Layout to transition from
		imageMemBarrier.newLayout = newLayout;							// Layout to transition to
		imageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	// Queue family to transition from
		imageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	// Queue family to transition to
		imageMemBarrier.image = image;									// image being aceesed and modified as part of barrier
		imageMemBarrier.subresourceRange.aspectMask = aspectFlag;		// Aspect of Image being altered
		imageMemBarrier.subresourceRange.baseMipLevel = 0;							// First mip level to start alterations on
		imageMemBarrier.subresourceRange.levelCount = 1;								// number of mip levels to alter starting from base mip level
		imageMemBarrier.subresourceRange.baseArrayLayer = 0;							// First layer to start alterations on
		imageMemBarrier.subresourceRange.layerCount = 1;								// number of layers to alter starting from base array layer

		VkPipelineStageFlags srcStage = {};
		VkPipelineStageFlags dstStage = {};

		// If transitioning from new image to image ready to  recevie data...
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			imageMemBarrier.srcAccessMask = 0;								// Memory access stage transition must after...
			imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	// Memory access stage transition must before...

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		// If transitioning from transfer dst to shader readable...
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			// Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
			imageMemBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			// Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// srcStage: 이미지가 depth/stencil attachment로 사용될 때 최종 프래그먼트 테스트 후에 접근 가능하도록 설정합니다.
			srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			// dstStage: 셰이더 단계에서 읽어야 하므로 프래그먼트 셰이더에 맞추어 설정합니다.
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			// Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
			imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			// Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// srcStage: Color attachment에 쓰는 단계인 color attachment output 단계에서 접근할 수 있도록 설정합니다.
			srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			// dstStage: 프래그먼트 셰이더에서 읽기 위해 프래그먼트 셰이더 단계로 설정합니다.
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier(
			transferCommandBuffer,
			srcStage, dstStage,					// Pipeline Stages (match to src and dst AccessMasks)
			0,									// Dependency flags
			0, nullptr,							// Memory Barreir count + data
			0, nullptr,							// Buffer Memory Barrier count + data
			1, &imageMemBarrier					// Image Memory Barrier count + data
		);

		// End commands
		vkEndCommandBuffer(transferCommandBuffer);

		// Queue submission information
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transferCommandBuffer;

		// Submit Transfer command to transfer queue and wait until it finishes
		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(transferQueue); // 큐가 Idle 상태가 될 때까지 기다린다. 여기서 Idle 상태란, 대기 상태에 있는 것을 이야기한다.

		// Free Temporary command buffer back to pool
		vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
	}

	static void CmdImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectFlag)
	{
		VkImageMemoryBarrier imageMemBarrier = {};
		imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemBarrier.oldLayout = oldLayout;							// Layout to transition from
		imageMemBarrier.newLayout = newLayout;							// Layout to transition to
		imageMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	// Queue family to transition from
		imageMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	// Queue family to transition to
		imageMemBarrier.image = image;									// image being aceesed and modified as part of barrier
		imageMemBarrier.subresourceRange.aspectMask = aspectFlag;		// Aspect of Image being altered
		imageMemBarrier.subresourceRange.baseMipLevel = 0;							// First mip level to start alterations on
		imageMemBarrier.subresourceRange.levelCount = 1;								// number of mip levels to alter starting from base mip level
		imageMemBarrier.subresourceRange.baseArrayLayer = 0;							// First layer to start alterations on
		imageMemBarrier.subresourceRange.layerCount = 1;								// number of layers to alter starting from base array layer

		VkPipelineStageFlags srcStage = {};
		VkPipelineStageFlags dstStage = {};

		// If transitioning from new image to image ready to  recevie data...
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			imageMemBarrier.srcAccessMask = 0;								// Memory access stage transition must after...
			imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	// Memory access stage transition must before...

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		// If transitioning from transfer dst to shader readable...
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			// Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
			imageMemBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			// Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// srcStage: 이미지가 depth/stencil attachment로 사용될 때 최종 프래그먼트 테스트 후에 접근 가능하도록 설정합니다.
			srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			// dstStage: 셰이더 단계에서 읽어야 하므로 프래그먼트 셰이더에 맞추어 설정합니다.
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			// Depth attachment로 쓰일 때 접근 권한: Depth attachment에 대한 읽기 및 쓰기 접근이 필요합니다.
			imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			// Shader read-only 레이아웃으로 전환 후에는 셰이더 단계에서 읽기만 필요합니다.
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// srcStage: Color attachment에 쓰는 단계인 color attachment output 단계에서 접근할 수 있도록 설정합니다.
			srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			// dstStage: 프래그먼트 셰이더에서 읽기 위해 프래그먼트 셰이더 단계로 설정합니다.
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			srcStage, dstStage,					// Pipeline Stages (match to src and dst AccessMasks)
			0,									// Dependency flags
			0, nullptr,							// Memory Barreir count + data
			0, nullptr,							// Buffer Memory Barrier count + data
			1, &imageMemBarrier					// Image Memory Barrier count + data
		);
	}
}