#pragma once

/* Graphics API Abstraction */
#include "typedef.h"
#include "GfxDevice.h"
#include "GfxImage.h"
#include "GfxBuffer.h"
#include "GfxDescriptor.h"
#include "GfxPipeline.h"
#include "GfxRenderPass.h"
#include "GfxFrameBuffer.h"
#include "GfxCommandPool.h"
#include "GfxCommandBuffer.h"
#include "GfxResourceManager.h"

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};