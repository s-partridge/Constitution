#ifndef CGE_RESOURCE_LOADER_H
#define CGE_RESOURCE_LOADER_H

#include "vulkanBootstrapper.h"

namespace cge
{
	VkShaderModule loadVulkanShaderModule(const cge::vulkan::DeviceFunctions &deviceFunctions, VkDevice device, const char *path);
}

#endif // CGE_RESOURCE_LOADER_H