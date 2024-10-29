#include "GfxCommandPool.h"

GfxCommandPool& GfxCommandPool::Initialize(VkDevice newDevice, uint32_t newQueueFamilyIndex)
{
    device = newDevice;
    queueFamilyIndex = newQueueFamilyIndex;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;			// Queue family type that buffers from this command pool will use

    // Create a Graphics Queue family Command Pool
    VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS)
    {
        assert(false && "Failed to create Command Pool!");
    }

    return *this;
}

void GfxCommandPool::Destroy()
{
    vkDestroyCommandPool(device, commandPool, nullptr);
}

GfxCommandBuffer* GfxCommandPool::Allocate()
{
    commandBuffers.push_back(std::make_shared<GfxCommandBuffer>());
    commandBuffers[commandBuffers.size() - 1]->Initialize(device, this);

    return commandBuffers.back().get();
}

bool GfxCommandPool::Free(GfxCommandBuffer* commandBuffer)
{
    if (commandBuffer)
    {
        for (size_t i = 0; i < commandBuffers.size(); ++i)
        {
            if (commandBuffers[i].get() == commandBuffer)
            {
                commandBuffers[i].get()->Destroy(this);
                commandBuffers.erase(commandBuffers.begin() + i);
                commandBuffer = nullptr;

                return true;
            }
        }
    }
    return false;
}

const uint32_t GfxCommandPool::GetQueueFamily()
{
    return queueFamilyIndex;
}

VkCommandPool& GfxCommandPool::GetVkCommandPool()
{
    return commandPool;
}
