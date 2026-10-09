#include "vulkanBootstrapper.h"

namespace cge::vulkan
{
	bool loadGlobalfunctions(InstanceFunctions &instanceFunctions, PFN_vkGetInstanceProcAddr getInstanceProcAddr)
	{
		instanceFunctions.vkGetInstanceProcAddr = getInstanceProcAddr;
		instanceFunctions.vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(getInstanceProcAddr(nullptr, "vkCreateInstance"));

		return instanceFunctions.vkCreateInstance != nullptr;
	}

	bool loadInstanceFunctions(InstanceFunctions &instanceFunctions, VkInstance instance)
	{
		//don't autocomplete'
		instanceFunctions.vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
		instanceFunctions.vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
		instanceFunctions.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
		instanceFunctions.vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
		instanceFunctions.vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkCreateDevice"));

		return instanceFunctions.vkDestroyInstance != nullptr &&
			instanceFunctions.vkEnumeratePhysicalDevices != nullptr &&
			instanceFunctions.vkGetPhysicalDeviceProperties != nullptr &&
			instanceFunctions.vkGetDeviceProcAddr != nullptr &&
			instanceFunctions.vkCreateDevice != nullptr;
	}

	bool loadDeviceFunctions(DeviceFunctions &deviceFunctions, InstanceFunctions &instanceFunctions, VkDevice device)
	{
		deviceFunctions.vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyDevice"));
		deviceFunctions.vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(instanceFunctions.vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));

		return deviceFunctions.vkDestroyDevice != nullptr &&
			deviceFunctions.vkGetDeviceQueue != nullptr;
	}
}