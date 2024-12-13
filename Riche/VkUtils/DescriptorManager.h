#pragma once
#include "Utils/Singleton.h"

namespace VkUtils
{
	class DescriptorBuilder;

	using DescriptorHandle = uint64_t;
	inline constexpr DescriptorHandle const INVALID_DESC_HANDLE = uint64_t(-1);

	class DescriptorManager : public Singleton<DescriptorManager>
	{
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

		DescriptorHandle AddDescriptorSet(DescriptorBuilder* builder, std::string const& name);
		VkDescriptorSet& GetVkDescriptorSet(DescriptorHandle handle);
		VkDescriptorSet& GetVkDescriptorSet(std::string const& name);

		VkDescriptorSetLayout& GetVkDescriptorSetLayout(DescriptorHandle handle);
		VkDescriptorSetLayout& GetVkDescriptorSetLayout(std::string const& name);
	};
}

#define g_DescriptorManager VkUtils::DescriptorManager::Get()