#include "DescriptorManager.h"
#include "DescriptorBuilder.h"

namespace VkUtils
{
	void DescriptorManager::Initialize()
	{
	}

	DescriptorHandle DescriptorManager::AddDescriptorSet(DescriptorBuilder* builder, std::string const& name)
	{
		if (auto iter = loadedDescriptorSet.find(name) == loadedDescriptorSet.end())
		{
			++setHandle;
			++setLayoutHandle;

			VkDescriptorSet set;
			VkDescriptorSetLayout setLayout;

			builder->Build(set, setLayout);
			loadedDescriptorSet.insert({ name, setHandle });
			loadedSetLayout.insert({ name, setLayoutHandle });
			descriptorSetMap[setHandle] = set;
			setLayoutMap[setLayoutHandle] = setLayout;
		}
		return setHandle;
	}

	VkDescriptorSet& DescriptorManager::GetVkDescriptorSet(DescriptorHandle handle)
	{
		return descriptorSetMap.find(handle)->second;
	}

	VkDescriptorSet& DescriptorManager::GetVkDescriptorSet(std::string const& name)
	{
		return descriptorSetMap.find(loadedDescriptorSet.find(name)->second)->second;
	}

	VkDescriptorSetLayout& DescriptorManager::GetVkDescriptorSetLayout(DescriptorHandle handle)
	{
		return setLayoutMap.find(handle)->second;
	}

	VkDescriptorSetLayout& DescriptorManager::GetVkDescriptorSetLayout(std::string const& name)
	{
		return setLayoutMap.find(loadedSetLayout.find(name)->second)->second;
	}
}