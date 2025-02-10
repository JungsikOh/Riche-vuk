#pragma once
#include "Utils/Singleton.h"

namespace VkUtils {
class DescriptorBuilder;

using DescriptorHandle = uint64_t;
inline constexpr DescriptorHandle const INVALID_DESC_HANDLE = uint64_t(-1);

class DescriptorManager : public Singleton<DescriptorManager> {
  friend class Singleton<DescriptorManager>;

 private:
  DescriptorHandle setHandle = INVALID_DESC_HANDLE;
  DescriptorHandle setLayoutHandle = INVALID_DESC_HANDLE;

  std::unordered_map<DescriptorHandle, VkDescriptorSet> descriptorSetMap{};
  std::unordered_map<std::string, DescriptorHandle> loadedDescriptorSet{};

  std::unordered_map<DescriptorHandle, VkDescriptorSetLayout> setLayoutMap{};
  std::unordered_map<std::string, DescriptorHandle> loadedSetLayout{};

 public:
  DescriptorManager() = default;
  DescriptorManager(DescriptorManager const&) = delete;
  DescriptorManager& operator=(DescriptorManager const&) = delete;
  ~DescriptorManager() = default;

  void Initialize();
  void Cleanup(VkDevice device);

  DescriptorHandle AddDescriptorSet(DescriptorBuilder* builder, std::string const& name, bool isBindless = false);
  VkDescriptorSet& GetVkDescriptorSet(DescriptorHandle handle);
  VkDescriptorSet& GetVkDescriptorSet(std::string const& name);

  VkDescriptorSetLayout& GetVkDescriptorSetLayout(DescriptorHandle handle);
  VkDescriptorSetLayout& GetVkDescriptorSetLayout(std::string const& name);
};

static VkWriteDescriptorSet& WriteDescriptorSet(VkDescriptorSet set, VkDescriptorType type, uint32_t binding,
                                               VkDescriptorImageInfo* info) {
  VkWriteDescriptorSet newWrite{};
  newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  newWrite.pNext = nullptr;
  newWrite.dstSet = set;

  newWrite.descriptorCount = 1;
  newWrite.descriptorType = type;
  newWrite.pImageInfo = info;
  newWrite.dstBinding = binding;

  return newWrite;
};

static VkWriteDescriptorSet& WriteDescriptorSet(VkDescriptorSet set, VkDescriptorType type, uint32_t binding,
                                               VkDescriptorBufferInfo* info) {
  VkWriteDescriptorSet newWrite{};
  newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  newWrite.pNext = nullptr;
  newWrite.dstSet = set;

  newWrite.descriptorCount = 1;
  newWrite.descriptorType = type;
  newWrite.pBufferInfo = info;
  newWrite.dstBinding = binding;

  return newWrite;
};

static VkDescriptorSetLayoutBinding& DescriptorSetLayoutBinding(uint32_t binding, VkDescriptorType type,
                                                               VkShaderStageFlags stageFlags) {
  VkDescriptorSetLayoutBinding newBinding{};
  newBinding.descriptorCount = 1;
  newBinding.descriptorType = type;
  newBinding.pImmutableSamplers = nullptr;
  newBinding.stageFlags = stageFlags;
  newBinding.binding = binding;

  return newBinding;
};

}  // namespace VkUtils

#define g_DescriptorManager VkUtils::DescriptorManager::Get()