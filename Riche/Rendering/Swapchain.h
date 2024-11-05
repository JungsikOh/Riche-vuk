#pragma once

struct SwapChainImage
{
	VkImage image;
	VkImageView imageView;
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities;			// surface properites, e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;				// surface image foramts, e.g. RGBA and size of each color
	std::vector<VkPresentModeKHR> presentationModes;		// how images should be presented to screen, e.g. IMMEDAITE and MAILBOX etc..
};