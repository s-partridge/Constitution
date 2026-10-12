#include "vulkanBootstrapper.h"

namespace cge::vulkan
{
	bool loadGlobalfunctions(VulkanFunctions &instanceFunctions, PFN_vkGetInstanceProcAddr getInstanceProcAddr)
	{
		// Load global functions that are required before creating a Vulkan instance
		instanceFunctions.vkGetInstanceProcAddr = getInstanceProcAddr;
		instanceFunctions.vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(getInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
		instanceFunctions.vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(getInstanceProcAddr(nullptr, "vkCreateInstance"));

		// Ensure that all required global functions were successfully loaded
		return instanceFunctions.vkEnumerateInstanceLayerProperties != nullptr &&
			instanceFunctions.vkCreateInstance != nullptr;
	}

	bool loadInstanceFunctions(VulkanFunctions &instanceFunctions, VkInstance instance)
	{
		// Load instance-level functions
		instanceFunctions.vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
		instanceFunctions.vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
		
		// Load physical device functions
		instanceFunctions.vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
		instanceFunctions.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
		instanceFunctions.vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
		instanceFunctions.vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkCreateDevice"));
		
		// Load surface functions
		instanceFunctions.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
		instanceFunctions.vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
		
		// Load queue family functions
		instanceFunctions.vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
		instanceFunctions.vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(instanceFunctions.vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));

		// Ensure that all required instance functions were successfully loaded
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
		// Load device-level functions
		deviceFunctions.vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyDevice"));
		deviceFunctions.vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(instanceFunctions.vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));
		// Load swapchain functions
		deviceFunctions.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
		deviceFunctions.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
		// Load swapchain image functions
		deviceFunctions.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
		deviceFunctions.vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateImageView"));
		deviceFunctions.vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyImageView"));
		// Load command pool and command buffer functions
		deviceFunctions.vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
		deviceFunctions.vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
		deviceFunctions.vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(instanceFunctions.vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
		// Load synchronization primitives
		deviceFunctions.vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
		deviceFunctions.vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroySemaphore"));
		deviceFunctions.vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateFence"));
		deviceFunctions.vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyFence"));

		// Load render-time synchronization functions
		deviceFunctions.vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(instanceFunctions.vkGetDeviceProcAddr(device, "vkWaitForFences"));
		deviceFunctions.vkResetFences = reinterpret_cast<PFN_vkResetFences>(instanceFunctions.vkGetDeviceProcAddr(device, "vkResetFences"));
		deviceFunctions.vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));

		// Load render-time command buffer functions
		deviceFunctions.vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(instanceFunctions.vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
		deviceFunctions.vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(instanceFunctions.vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
		deviceFunctions.vkCmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2"));
		deviceFunctions.vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCmdBeginRendering"));
		deviceFunctions.vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCmdEndRendering"));

		// Load render-time queue functions
		deviceFunctions.vkQueueSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2>(instanceFunctions.vkGetDeviceProcAddr(device, "vkQueueSubmit2"));
		deviceFunctions.vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(instanceFunctions.vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));

		// Load device idle function
		deviceFunctions.vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDeviceWaitIdle"));

		// Ensure that all required device functions were successfully loaded
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
			deviceFunctions.vkDestroyFence != nullptr &&
			deviceFunctions.vkWaitForFences != nullptr &&
			deviceFunctions.vkResetFences != nullptr &&
			deviceFunctions.vkAcquireNextImageKHR != nullptr &&
			deviceFunctions.vkBeginCommandBuffer != nullptr &&
			deviceFunctions.vkEndCommandBuffer != nullptr &&
			deviceFunctions.vkCmdPipelineBarrier2 != nullptr &&
			deviceFunctions.vkCmdBeginRendering != nullptr &&
			deviceFunctions.vkCmdEndRendering != nullptr &&
			deviceFunctions.vkQueueSubmit2 != nullptr &&
			deviceFunctions.vkQueuePresentKHR != nullptr &&
			deviceFunctions.vkDeviceWaitIdle != nullptr;
	}
}