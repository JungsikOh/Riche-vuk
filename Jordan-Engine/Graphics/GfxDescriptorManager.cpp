#include "GfxDescriptorManager.h"
#include "GfxDescriptor.h"

DescriptorHandle GfxDescriptorManager::SetDescriptorSet(GfxDescriptorBuilder& builder, VkDescriptorSetLayout& layout)
{
    ++handle;

    VkDescriptorSet set;
    builder.Build(set, layout);

    descriptorSetMap.insert({ handle, set });
    return handle;
}

VkDescriptorSet GfxDescriptorManager::GetDescriptorSet(DescriptorHandle descriptorHandle) const
{
    return descriptorSetMap.find(descriptorHandle)->second;
}
