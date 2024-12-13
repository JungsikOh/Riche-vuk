#include "DescriptorBuilder.h"

namespace VkUtils
{
	////////////////////////////////////
	// DescriptorAllocator
	////////////////////////////////////

	void DescriptorAllocator::ResetPools()
	{
		for (auto p : usedPools)
		{
			vkResetDescriptorPool(device, p, 0);
			freePools.push_back(p);
		}

		usedPools.clear();
		currentPool = VK_NULL_HANDLE;
	}

	bool DescriptorAllocator::Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
	{
		if (currentPool == VK_NULL_HANDLE)
		{
			currentPool = GrapPool();
			usedPools.push_back(currentPool);
		}

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;

		allocInfo.pSetLayouts = &layout;
		allocInfo.descriptorPool = currentPool;
		allocInfo.descriptorSetCount = 1;

		VkResult result = vkAllocateDescriptorSets(device, &allocInfo, set);
		bool needReallocate = false;

		switch (result)
		{
		case VK_SUCCESS:
			return true;
		case VK_ERROR_FRAGMENTED_POOL:
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			needReallocate = true;
			break;
		default:
			return false;
		}

		if (needReallocate) {
			//allocate a new pool and retry
			currentPool = GrapPool();
			usedPools.push_back(currentPool);

			result = vkAllocateDescriptorSets(device, &allocInfo, set);

			//if it still fails then we have big issues
			if (result == VK_SUCCESS) {
				return true;
			}
		}
		return false;
	}

	void DescriptorAllocator::Initialize(VkDevice newDevice)
	{
		device = newDevice;
	}

	void DescriptorAllocator::Cleanup()
	{
		for (auto p : freePools)
		{
			vkDestroyDescriptorPool(device, p, nullptr);
		}
		for (auto p : usedPools)
		{
			vkDestroyDescriptorPool(device, p, nullptr);
		}
	}

	VkDescriptorPool DescriptorAllocator::GrapPool()
	{
		// there are reusable pools available
		if (freePools.size() > 0)
		{
			// Grap pool from the back of the vector and remove it from there
			VkDescriptorPool pool = freePools.back();
			freePools.pop_back();
			return pool;
		}
		else
		{
			return CreatePool(device, descriptorSizes, 1000, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
		}
	}

	////////////////////////////////////
	// DescriptorLayoutCache
	////////////////////////////////////

	void DescriptorLayoutCache::Initialize(VkDevice newDevice)
	{
		device = newDevice;
	}

	void DescriptorLayoutCache::Cleanup()
	{
		for (auto pair : layoutCache)
		{
			vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
		}
	}

	VkDescriptorSetLayout DescriptorLayoutCache::CreateDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info)
	{
		DescriptorLayoutInfo layoutInfo;
		layoutInfo.bindings.reserve(info->bindingCount);
		bool isSorted = true;
		int lastBinding = -1;

		for (int i = 0; i < info->bindingCount; ++i)
		{
			layoutInfo.bindings.push_back(info->pBindings[i]);

			if (info->pBindings[i].binding > lastBinding)
			{
				lastBinding = info->pBindings[i].binding;
			}
			else
			{
				isSorted = false;
			}
		}

		if (!isSorted)
		{
			std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
				return a.binding < b.binding;
				});
		}

		// try to grap from cache
		auto iter = layoutCache.find(layoutInfo);
		if (iter != layoutCache.end())
		{
			return (*iter).second;
		}
		else
		{
			// Not Found
			VkDescriptorSetLayout layout;
			VkResult result = vkCreateDescriptorSetLayout(device, info, nullptr, &layout);
			if (result != VK_SUCCESS)
			{
				assert(false && "failed to create descriptor set layout");
			}

			layoutCache[layoutInfo] = layout;
			return layout;
		}
	}

	bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
	{
		if (other.bindings.size() != bindings.size()) {
			return false;
		}
		else {
			//compare each of the bindings is the same. Bindings are sorted so they will match
			for (int i = 0; i < bindings.size(); i++) {
				if (other.bindings[i].binding != bindings[i].binding) {
					return false;
				}
				if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
					return false;
				}
				if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
					return false;
				}
				if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
					return false;
				}
			}
			return true;
		}
	}

	size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
	{
		size_t result = std::hash<size_t>()(bindings.size());

		for (const VkDescriptorSetLayoutBinding& binding : bindings)
		{
			//pack the binding data into a single int64. Not fully correct but it's ok
			size_t binding_hash = binding.binding | binding.descriptorType << 8 | binding.descriptorCount << 16 | binding.stageFlags << 24;

			//shuffle the packed binding data and xor it with the main hash
			result ^= std::hash<size_t>()(binding_hash);
		}

		return result;
	}

	////////////////////////////////////
	// DescriptorBuilder
	////////////////////////////////////

	DescriptorBuilder DescriptorBuilder::Begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator)
	{
		DescriptorBuilder builder;

		builder.cache = layoutCache;
		builder.alloc = allocator;
		return builder;
	}

	DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		//create the descriptor binding for the layout
		VkDescriptorSetLayoutBinding newBinding{};

		newBinding.descriptorCount = 1;
		newBinding.descriptorType = type;
		newBinding.pImmutableSamplers = nullptr;
		newBinding.stageFlags = stageFlags;
		newBinding.binding = binding;

		bindings.push_back(newBinding);

		//create the descriptor write
		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = 1;
		newWrite.descriptorType = type;
		newWrite.pBufferInfo = bufferInfo;
		newWrite.dstBinding = binding;

		writes.push_back(newWrite);
		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		//create the descriptor binding for the layout
		VkDescriptorSetLayoutBinding newBinding{};

		newBinding.descriptorCount = 1;
		newBinding.descriptorType = type;
		newBinding.pImmutableSamplers = nullptr;
		newBinding.stageFlags = stageFlags;
		newBinding.binding = binding;

		bindings.push_back(newBinding);

		//create the descriptor write
		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = 1;
		newWrite.descriptorType = type;
		newWrite.pImageInfo = imageInfo;
		newWrite.dstBinding = binding;

		writes.push_back(newWrite);
		return *this;
	}

	bool DescriptorBuilder::Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
	{
		//build layout first
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = nullptr;

		layoutInfo.pBindings = bindings.data();
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

		layout = cache->CreateDescriptorLayout(&layoutInfo);

		//allocate descriptor
		bool success = alloc->Allocate(&set, layout);
		if (!success) { return false; };

		//write descriptor
		for (VkWriteDescriptorSet& w : writes) {
			w.dstSet = set;
		}

		vkUpdateDescriptorSets(alloc->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

		return true;
	}

	bool DescriptorBuilder::Build(VkDescriptorSet& set)
	{
		//write descriptor
		for (VkWriteDescriptorSet& w : writes) {
			w.dstSet = set;
		}

		vkUpdateDescriptorSets(alloc->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

		return true;
	}
}