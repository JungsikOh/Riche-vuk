#include "DescriptorBuilder.h"

namespace VkUtils {
////////////////////////////////////
// DescriptorAllocator
////////////////////////////////////

void DescriptorAllocator::ResetPools() {
  for (auto p : usedPools) {
    vkResetDescriptorPool(device, p, 0);
    freePools.push_back(p);
  }

  usedPools.clear();
  currentPool = VK_NULL_HANDLE;
}

bool DescriptorAllocator::Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout) {
  if (currentPool == VK_NULL_HANDLE) {
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

  switch (result) {
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
    // allocate a new pool and retry
    currentPool = GrapPool();
    usedPools.push_back(currentPool);

    result = vkAllocateDescriptorSets(device, &allocInfo, set);

    // if it still fails then we have big issues
    if (result == VK_SUCCESS) {
      return true;
    }
  }
  return false;
}

bool DescriptorAllocator::AllocateBindless(VkDescriptorSet* outSet, VkDescriptorSetLayout layout, uint32_t descriptorCount) {
  if (currentPool == VK_NULL_HANDLE) {
    currentPool = GrapPool();  // create or fetch
    usedPools.push_back(currentPool);
  }

  // variable descriptor count info
  VkDescriptorSetVariableDescriptorCountAllocateInfoEXT varCountInfo{};
  varCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
  varCountInfo.descriptorSetCount = 1;
  varCountInfo.pDescriptorCounts = &descriptorCount;

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = currentPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;
  allocInfo.pNext = &varCountInfo;

  VkResult result = vkAllocateDescriptorSets(device, &allocInfo, outSet);
  bool needReallocate = false;

  switch (result) {
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
    // allocate a new pool and retry
    currentPool = GrapPool();
    usedPools.push_back(currentPool);

    result = vkAllocateDescriptorSets(device, &allocInfo, outSet);

    // if it still fails then we have big issues
    if (result == VK_SUCCESS) {
      return true;
    }
  }
  return false;
}

void DescriptorAllocator::Initialize(VkDevice newDevice) { device = newDevice; }

void DescriptorAllocator::Cleanup() {
  for (auto p : freePools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  for (auto p : usedPools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
}

VkDescriptorPool DescriptorAllocator::GrapPool() {
  // there are reusable pools available
  if (freePools.size() > 0) {
    // Grap pool from the back of the vector and remove it from there
    VkDescriptorPool pool = freePools.back();
    freePools.pop_back();
    return pool;
  } else {
    return CreatePool(device, descriptorSizes, 1000,
                      VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT);
  }
}

////////////////////////////////////
// DescriptorLayoutCache
////////////////////////////////////

void DescriptorLayoutCache::Initialize(VkDevice newDevice) { device = newDevice; }

void DescriptorLayoutCache::Cleanup() {
  for (auto pair : layoutCache) {
    vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
  }
}

VkDescriptorSetLayout DescriptorLayoutCache::CreateDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info) {
  DescriptorLayoutInfo layoutInfo;
  layoutInfo.bindings.reserve(info->bindingCount);
  bool isSorted = true;
  int lastBinding = -1;

  for (int i = 0; i < info->bindingCount; ++i) {
    layoutInfo.bindings.push_back(info->pBindings[i]);

    if (info->pBindings[i].binding > lastBinding) {
      lastBinding = info->pBindings[i].binding;
    } else {
      isSorted = false;
    }
  }

  if (!isSorted) {
    std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(),
              [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) { return a.binding < b.binding; });
  }

  // try to grap from cache
  auto iter = layoutCache.find(layoutInfo);
  if (iter != layoutCache.end()) {
    return (*iter).second;
  } else {
    // Not Found
    VkDescriptorSetLayout layout;
    VkResult result = vkCreateDescriptorSetLayout(device, info, nullptr, &layout);
    if (result != VK_SUCCESS) {
      assert(false && "failed to create descriptor set layout");
    }

    layoutCache[layoutInfo] = layout;
    return layout;
  }
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const {
  if (other.bindings.size() != bindings.size()) {
    return false;
  } else {
    // compare each of the bindings is the same. Bindings are sorted so they will match
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

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
  size_t result = std::hash<size_t>()(bindings.size());

  for (const VkDescriptorSetLayoutBinding& binding : bindings) {
    // pack the binding data into a single int64. Not fully correct but it's ok
    size_t binding_hash = binding.binding | binding.descriptorType << 8 | binding.descriptorCount << 16 | binding.stageFlags << 24;

    // shuffle the packed binding data and xor it with the main hash
    result ^= std::hash<size_t>()(binding_hash);
  }

  return result;
}

////////////////////////////////////
// DescriptorBuilder
////////////////////////////////////

DescriptorBuilder DescriptorBuilder::Begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator) {
  DescriptorBuilder builder;

  builder.cache = layoutCache;
  builder.alloc = allocator;
  return builder;
}

DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type,
                                                 VkShaderStageFlags stageFlags, bool isBindless /*= false*/) {
  // create the descriptor binding for the layout
  VkDescriptorSetLayoutBinding newBinding{};

  newBinding.descriptorCount = isBindless ? 1000 : 1;
  newBinding.descriptorType = type;
  newBinding.pImmutableSamplers = nullptr;
  newBinding.stageFlags = stageFlags;
  newBinding.binding = binding;

  bindings.push_back(newBinding);

  // create the descriptor write
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

DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type,
                                                VkShaderStageFlags stageFlags, bool isBindless /*= false*/, int count) {
  // create the descriptor binding for the layout
  VkDescriptorSetLayoutBinding newBinding{};

  newBinding.descriptorCount = isBindless ? 1000 : 1;
  newBinding.descriptorType = type;
  newBinding.pImmutableSamplers = nullptr;
  newBinding.stageFlags = stageFlags;
  newBinding.binding = binding;

  bindings.push_back(newBinding);

  // create the descriptor write
  VkWriteDescriptorSet newWrite{};
  newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  newWrite.pNext = nullptr;

  newWrite.descriptorCount = isBindless ? count : 1;
  newWrite.descriptorType = type;
  newWrite.pImageInfo = imageInfo;
  newWrite.dstBinding = binding;

  writes.push_back(newWrite);
  return *this;
}

bool DescriptorBuilder::Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout, bool isBindless /* = false */) {
  // build layout first
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.pNext = nullptr;

  layoutInfo.pBindings = bindings.data();
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT flagsCreateInfo{};
  std::vector<VkDescriptorBindingFlags> bindingFlags;
  if (isBindless) {
    // VkDescriptorBindingFlagsEXT bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
    //                                             VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
    //                                             VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;

    // flagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    // flagsCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    // flagsCreateInfo.pBindingFlags = &bindlessFlags;

    bindingFlags.resize(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                             VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                             VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

    flagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    flagsCreateInfo.pBindingFlags = bindingFlags.data();

    layoutInfo.pNext = &flagsCreateInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  }

  layout = cache->CreateDescriptorLayout(&layoutInfo);

  // allocate descriptor
  bool success = isBindless ? alloc->AllocateBindless(&set, layout, 1000) : alloc->Allocate(&set, layout);
  if (!success) {
    return false;
  };

  // write descriptor
  for (VkWriteDescriptorSet& w : writes) {
    w.dstSet = set;
  }

  vkUpdateDescriptorSets(alloc->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

  return true;
}

bool DescriptorBuilder::Build(VkDescriptorSet& set) {
  // write descriptor
  for (VkWriteDescriptorSet& w : writes) {
    w.dstSet = set;
  }

  vkUpdateDescriptorSets(alloc->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

  return true;
}
}  // namespace VkUtils