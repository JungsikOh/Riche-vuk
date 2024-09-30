#pragma once
#include "GfxCore.h"

using DescriptorHandle = uint64_t;
inline constexpr DescriptorHandle const INVAILD_DESCRIPTOR_HANDLE = uint64_t(-1);

class GfxDescriptorBuilder;
class GfxDescriptorManager
{
public:
	void Initialize();
	void Destory();

	DescriptorHandle SetDescriptorSet(GfxDescriptorBuilder& builder);
	VkDescriptorSet GetDescriptorSet(DescriptorHandle descriptorHandle) const;

private:
	DescriptorHandle handle = INVAILD_DESCRIPTOR_HANDLE;
	std::unordered_map<DescriptorHandle, VkDescriptorSet> descriptorSetMap{};
};

