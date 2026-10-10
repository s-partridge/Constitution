#include "vulkanBootstrapper.h"

namespace cge::vulkan
{
	bool loadGlobalfunctions(VulkanFunctions &instanceFunctions, PFN_vkGetInstanceProcAddr getInstanceProcAddr)
	{
		instanceFunctions.vkGetInstanceProcAddr = getInstanceProcAddr;
		instanceFunctions.vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(getInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
		instanceFunctions.vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(getInstanceProcAddr(nullptr, "vkCreateInstance"));
		return instanceFunctions.vkEnumerateInstanceLayerProperties != nullptr &&
			instanceFunctions.vkCreateInstance != nullptr;
	}

	bool loadInstanceFunctions(VulkanFunctions &instanceFunctions, VkInstance instance)
	{
		instanceFunctions.vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
		instanceFunctions.vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
		instanceFunctions.vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
		instanceFunctions.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
		instanceFunctions.vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
		instanceFunctions.vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkCreateDevice"));

		instanceFunctions.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
		instanceFunctions.vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));

		instanceFunctions.vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
		instanceFunctions.vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));

		return instanceFunctions.vkDestroyInstance != nullptr &&
			instanceFunctions.vkDestroySurfaceKHR != nullptr &&
			instanceFunctions.vkEnumeratePhysicalDevices != nullptr &&
			instanceFunctions.vkGetPhysicalDeviceProperties != nullptr &&
			instanceFunctions.vkGetDeviceProcAddr != nullptr &&
			instanceFunctions.vkCreateDevice != nullptr &&
			instanceFunctions.vkGetPhysicalDeviceSurfaceCapabilitiesKHR &&
			instanceFunctions.vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr &&
			instanceFunctions.vkGetPhysicalDeviceQueueFamilyProperties != nullptr &&
			instanceFunctions.vkGetPhysicalDeviceSurfaceSupportKHR != nullptr;
	}

	bool loadDeviceFunctions(DeviceFunctions &deviceFunctions, VulkanFunctions &instanceFunctions, VkDevice device)
	{
		deviceFunctions.vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyDevice"));
		deviceFunctions.vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(instanceFunctions.vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));

		deviceFunctions.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
		deviceFunctions.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));

		deviceFunctions.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
		deviceFunctions.vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateImageView"));
		deviceFunctions.vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyImageView"));

		deviceFunctions.vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
		deviceFunctions.vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
		deviceFunctions.vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(instanceFunctions.vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));

		deviceFunctions.vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
		deviceFunctions.vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
		deviceFunctions.vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateFence"));
		deviceFunctions.vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyFence"));

		return deviceFunctions.vkDestroyDevice != nullptr &&
			deviceFunctions.vkGetDeviceQueue != nullptr &&
			deviceFunctions.vkCreateSwapchainKHR != nullptr &&
			deviceFunctions.vkDestroySwapchainKHR != nullptr &&
			deviceFunctions.vkGetSwapchainImagesKHR != nullptr &&
			deviceFunctions.vkCreateImageView != nullptr &&
			deviceFunctions.vkDestroyImageView != nullptr &&
			deviceFunctions.vkCreateCommandPool != nullptr &&
			deviceFunctions.vkDestroyCommandPool != nullptr &&
			deviceFunctions.vkAllocateCommandBuffers != nullptr &&
			deviceFunctions.vkCreateSemaphore != nullptr &&
			deviceFunctions.vkDestroySemaphore != nullptr &&
			deviceFunctions.vkCreateFence != nullptr &&
			deviceFunctions.vkDestroyFence != nullptr;
	}
}