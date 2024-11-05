#pragma once
namespace VkUtils
{
	// Indices (loctions) of Queue Families (if they exist at all)
	struct QueueFamilyIndices 
	{
		int transferFamily = -1;
		int computeFamily = -1;
		int graphicsFamily = -1;			// Loc of Graphics Queue Family
		int presentationFamily = -1;		// Loc of Presentation Queue Family

		// Check if queue families are vaild
		bool isVaild()
		{
			return transferFamily >= 0 && computeFamily >= 0 && graphicsFamily >= 0 && presentationFamily >= 0;
		}
	};

	static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;

		// Get all Queue Family Property info for the given device
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

		// Go Through each queue family and check if it has at least 1 of the required types of queue
		int i = 0;
		for (const auto& queueFamily : queueFamilyList)
		{
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				indices.transferFamily = i;		// if queue family is vaild, then get index
			}

			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				indices.computeFamily = i;		// if queue family is vaild, then get index
			}

			// First check if queue family has at least 1 queue in that family(could have no queues)
			// Queue can be multiple types defined through bitfield. need to bitwise AND with VK_QUEUE_*_BIT to check if has required type.
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;		// if queue family is vaild, then get index
			}

			// Check if Queue family supports presentation
			VkBool32 presentationSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
			// Check if queue is presentation type (can be both graphics and presentation)
			if (queueFamily.queueCount > 0 && presentationSupport)
			{
				indices.presentationFamily = i;	// if queue family is vaild, then get index
			}

			// Check if queue family indices are in a valid state, stop searching if so
			if (indices.isVaild())
			{
				break;
			}

			i++;
		}

		return indices;
	}
}