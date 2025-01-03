#pragma once

#include "Utils/Singleton.h"

namespace VkUtils {
class DescriptorAllocator : public Singleton<DescriptorAllocator> {
  friend class Singleton<DescriptorAllocator>;

 public:
  struct PoolSizes {
    std::vector<std::pair<VkDescriptorType, float>> sizes = {{VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
                                                             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
                                                             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
                                                             {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
                                                             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
                                                             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
                                                             {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
  };

  DescriptorAllocator() = default;
  ~DescriptorAllocator() = default;

  void ResetPools();
  bool Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

  void Initialize(VkDevice newDevice);
  void Cleanup();

  VkDevice device;

 private:
  VkDescriptorPool GrapPool();

  VkDescriptorPool currentPool{VK_NULL_HANDLE};
  PoolSizes descriptorSizes;
  std::vector<VkDescriptorPool> usedPools;
  std::vector<VkDescriptorPool> freePools;
};

static VkDescriptorPool CreatePool(VkDevice device, const DescriptorAllocator::PoolSizes& poolSizes, int count,
                                   VkDescriptorPoolCreateFlags flags) {
  std::vector<VkDescriptorPoolSize> sizes;
  sizes.reserve(poolSizes.sizes.size());
  for (auto size : poolSizes.sizes) {
    sizes.push_back({size.first, uint32_t(size.second * count)});
  }

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = flags;
  poolInfo.maxSets = count;
  poolInfo.poolSizeCount = (uint32_t)sizes.size();
  poolInfo.pPoolSizes = sizes.data();

  VkDescriptorPool descriptorPool;
  VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
  if (result != VK_SUCCESS) {
    assert(false && "failed to create decriptor pool!");
  }

  return descriptorPool;
}

static VkDescriptorPool CreateBindlessPool(VkDevice device, uint32_t maxSampledImages) {
  // UPDATE_AFTER_BIND �÷��� �ʿ�
  VkDescriptorPoolCreateFlags poolFlags =
      VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

  // Pool Size
  std::vector<VkDescriptorPoolSize> sizes;
  // SAMPLED_IMAGE�� �뷮���� �� ���
  sizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxSampledImages});
  // �ʿ��� ��� SAMPLER, STORAGE_IMAGE �� �߰�

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = poolFlags;
  poolInfo.maxSets = 1;  // bindless ��Ʈ 1���� ũ�� �� ���� ����
  poolInfo.poolSizeCount = (uint32_t)sizes.size();
  poolInfo.pPoolSizes = sizes.data();

  VkDescriptorPool descriptorPool;
  VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

  return descriptorPool;
}

class DescriptorLayoutCache : public Singleton<DescriptorLayoutCache> {
  friend class Singleton<DescriptorLayoutCache>;

 public:
  DescriptorLayoutCache() = default;
  ~DescriptorLayoutCache() = default;

  void Initialize(VkDevice newDevice);
  void Cleanup();

  VkDescriptorSetLayout CreateDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info);
  VkDescriptorSetLayout CreateBindlessLayout(uint32_t maxCount, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags);

  struct DescriptorLayoutInfo {
    // good idea to turn this into a inlined array
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    bool operator==(const DescriptorLayoutInfo& other) const;

    size_t hash() const;
  };

 private:
  struct DescriptorLayoutHash {
    std::size_t operator()(const DescriptorLayoutInfo& k) const { return k.hash(); }
  };

  std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layoutCache;
  VkDevice device;
};

class DescriptorBuilder {
 public:
  DescriptorBuilder() = default;
  ~DescriptorBuilder() = default;

  static DescriptorBuilder Begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator);

  DescriptorBuilder& BindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type,
                                VkShaderStageFlags stageFlags);
  DescriptorBuilder& BindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type,
                               VkShaderStageFlags stageFlags);

  bool Build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
  bool Build(VkDescriptorSet& set);

 private:
  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  DescriptorLayoutCache* cache;
  DescriptorAllocator* alloc;
};
}  // namespace VkUtils

#define g_DescriptorAllocator VkUtils::DescriptorAllocator::Get()
#define g_DescriptorLayoutCache VkUtils::DescriptorLayoutCache::Get()