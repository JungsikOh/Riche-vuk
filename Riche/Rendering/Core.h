#pragma once

inline bool VK_CHECK(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        throw std::exception();
    }
    return true;
}