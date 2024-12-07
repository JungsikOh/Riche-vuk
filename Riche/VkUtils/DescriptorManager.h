#pragma once

namespace VkUtils
{
	class DescriptorBuilder;

	using DescriptorHandle = uint64_t;
	inline constexpr DescriptorHandle const INVALID_DESC_HANDLE = uint64_t(-1);

	class DescriptorManager
	{
	private:
		DescriptorHandle setHandle = INVALID_DESC_HANDLE;
		DescriptorHandle setLayoutHandle = INVALID_DESC_HANDLE;

		std::unordered_map<DescriptorHandle, VkDescriptorSet> descriptorSetMap{};
		std::unordered_map<std::wstring, DescriptorHandle> loadedDescriptorSet{};

		std::unordered_map<DescriptorHandle, VkDescriptorSetLayout> setLayoutMap{};
		std::unordered_map<std::wstring, DescriptorHandle> loadedSetLayout{};

	public:
		DescriptorManager() = default;
		~DescriptorManager() = default;

		void Initialize();

		DescriptorHandle AddDescriptorSet(DescriptorBuilder* builder, std::wstring const& name);
		VkDescriptorSet& GetVkDescriptorSet(DescriptorHandle handle);
		VkDescriptorSet& GetVkDescriptorSet(std::wstring const& name);

		VkDescriptorSetLayout& GetVkDescriptorSetLayout(DescriptorHandle handle);
		VkDescriptorSetLayout& GetVkDescriptorSetLayout(std::wstring const& name);
	};
}