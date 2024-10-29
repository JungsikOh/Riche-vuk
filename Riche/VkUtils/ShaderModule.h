#pragma once
namespace VkUtils
{
	static std::vector<char> ReadFile(const std::string& filename)
	{
		// Open stream from given file
		// std::ios::binary tells stream to read file as binary
		// std::ios::ate tells stream to start reading from end of file
		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		// Check if file stream successfully opened
		if (!file.is_open())
		{
			assert(false && "Failed to open a file!");
		}

		// Get current read position and use to resize file buffer
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> fileBuffer(fileSize);

		// Move read position (seek to) the start of the file
		file.seekg(0);
		// Read the file data into the buffer (stream "fileSize" in total)
		file.read(fileBuffer.data(), fileSize);

		// Close stream
		file.close();

		return fileBuffer;
	}

	static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code)
	{
		// Shader Moduel Create infomation
		VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = code.size();										// size of code
		shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());		// pointer of code (of uint32_t pointer type)

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule));

		return shaderModule;
	}
}