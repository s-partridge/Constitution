#ifndef CGE_VULKAN_BOOTSTRAPPER_H
#define CGE_VULKAN_BOOTSTRAPPER_H

// Include this header before anything else that includes vulkan.h.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace cge::vulkan
{
	struct InstanceFunctions
	{
		// Initial lookup function to source the Vulkan instance.
		// Currently must be supplied by the user of this bootstrapper.
		// TODO: Implement internal calls directly at the OS level rather than requiring something like GLFW to provide this function pointer.
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
		PFN_vkCreateInstance vkCreateInstance = nullptr;

		PFN_vkDestroyInstance vkDestroyInstance = nullptr;
		PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
		PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
		PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
		PFN_vkCreateDevice vkCreateDevice = nullptr;
	};

	struct DeviceFunctions
	{
		PFN_vkDestroyDevice vkDestroyDevice = nullptr;
		PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
	};

	bool loadGlobalfunctions(InstanceFunctions &instanceFunctions, PFN_vkGetInstanceProcAddr getInstanceProcAddr);
	bool loadInstanceFunctions(InstanceFunctions &instanceFunctions, VkInstance instance);
	bool loadDeviceFunctions(DeviceFunctions &deviceFunctions, InstanceFunctions &instanceFunctions, VkDevice device);
}

#endif // CGE_VULKAN_BOOTSTRAPPER_HD
