#include "resourceLoader.h"

#include <cstdio>
#include <cstdint>

namespace cge
{
	VkShaderModule loadVulkanShaderModule(const cge::vulkan::DeviceFunctions &deviceFunctions, VkDevice device, const char *path)
	{
		// Ensure valid file size. For Vulkan shaders, the file size must be a multiple of 4 bytes (32 bits) since SPIR-V is defined in 32-bit words.
		// MSVC will bitch about fopen being unsafe. I don't care. We'll worry about this later.
		// TODO: Create fopen_cge to wrap both fopen_s and fopen internally, compiler dependent, and use that instead of fopen directly. This will allow us to avoid the MSVC warning while still using the standard C library.
		FILE *file = std::fopen(path, "rb");
		if(!file)
			return VK_NULL_HANDLE;

		std::fseek(file, 0, SEEK_END);
		long size = std::ftell(file);
		if(size <= 0 || size % 4 != 0)
		{
			std::fclose(file);
			return VK_NULL_HANDLE; // Invalid file size for a SPIR-V shader module.
		}
		//Rewind to the beginning of the file to read its contents.
		std::fseek(file, 0, SEEK_SET);

		std::size_t wordCount = size / 4;
		std::uint32_t *code = new std::uint32_t[wordCount];
		std::size_t wordsRead = std::fread(code, sizeof(std::uint32_t), wordCount, file);
		std::fclose(file);

		if(wordsRead != wordCount)
		{
			delete[] code;
			return VK_NULL_HANDLE; // Failed to read the expected number of words from the file.
		}

		VkShaderModuleCreateInfo moduleInfo = VkShaderModuleCreateInfo();
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.codeSize = (size_t)size; // This is in bytes even though we read it as 32-bit words, and size won't be negative at this point in the code.
		moduleInfo.pCode = code;

		VkShaderModule shaderModule;
		VkResult result = deviceFunctions.vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule);

		// The shader module creation function copies the code into the Vulkan driver,
		// so we need to free our local copy after creation.
		delete[] code;
		if(result != VK_SUCCESS)
		{
			return VK_NULL_HANDLE; // Failed to create shader module.
		}
		return shaderModule;
	}
}