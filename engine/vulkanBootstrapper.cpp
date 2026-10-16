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
			instanceFunctions.vkGetPhysicalDeviceSurfaceSupportKHR != nullptr &&
			instanceFunctions.vkGetPhysicalDeviceMemoryProperties != nullptr;
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

		// Load shader functions
		deviceFunctions.vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateShaderModule"));
		deviceFunctions.vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyShaderModule"));
		deviceFunctions.vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreatePipelineLayout"));
		deviceFunctions.vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout"));
		deviceFunctions.vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateGraphicsPipelines"));
		deviceFunctions.vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyPipeline"));
		deviceFunctions.vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCreateBuffer"));
		deviceFunctions.vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(instanceFunctions.vkGetDeviceProcAddr(device, "vkDestroyBuffer"));
		
		// Load memory functions
		deviceFunctions.vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(instanceFunctions.vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements"));
		
		deviceFunctions.vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(instanceFunctions.vkGetDeviceProcAddr(device, "vkAllocateMemory"));
		deviceFunctions.vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(instanceFunctions.vkGetDeviceProcAddr(device, "vkFreeMemory"));
		deviceFunctions.vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(instanceFunctions.vkGetDeviceProcAddr(device, "vkBindBufferMemory"));

		deviceFunctions.vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(instanceFunctions.vkGetDeviceProcAddr(device, "vkMapMemory"));
		deviceFunctions.vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(instanceFunctions.vkGetDeviceProcAddr(device, "vkUnmapMemory"));

		deviceFunctions.vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(instanceFunctions.vkGetDeviceProcAddr(device, "vkCmdCopyBuffer"));
		deviceFunctions.vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(instanceFunctions.vkGetDeviceProcAddr(device, "vkQueueWaitIdle"));

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
			deviceFunctions.vkDeviceWaitIdle != nullptr &&
			deviceFunctions.vkCreateShaderModule != nullptr &&
			deviceFunctions.vkDestroyShaderModule != nullptr &&
			deviceFunctions.vkCreatePipelineLayout != nullptr &&
			deviceFunctions.vkDestroyPipelineLayout != nullptr &&
			deviceFunctions.vkCreateGraphicsPipelines != nullptr &&
			deviceFunctions.vkDestroyPipeline != nullptr &&
			deviceFunctions.vkCreateBuffer != nullptr &&
			deviceFunctions.vkDestroyBuffer != nullptr &&
			deviceFunctions.vkGetBufferMemoryRequirements != nullptr &&
			deviceFunctions.vkAllocateMemory != nullptr &&
			deviceFunctions.vkFreeMemory != nullptr &&
			deviceFunctions.vkBindBufferMemory != nullptr &&
			deviceFunctions.vkMapMemory != nullptr &&
			deviceFunctions.vkUnmapMemory != nullptr &&
			deviceFunctions.vkCmdCopyBuffer != nullptr &&
			deviceFunctions.vkQueueWaitIdle != nullptr;
	}

	std::uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProperties, std::uint32_t allowedTypeBits, VkMemoryPropertyFlags wantedFlags)
	{
		for(std::uint32_t typeIdx = 0; typeIdx < memoryProperties.memoryTypeCount; ++typeIdx)
		{
			// A type is allowed if its bit is set in the allowedTypeBits bitmask
			bool allowed = (allowedTypeBits & (1 << typeIdx)) != 0;
			bool hasWantedFlags = (memoryProperties.memoryTypes[typeIdx].propertyFlags & wantedFlags) == wantedFlags;

			if(allowed && hasWantedFlags)
				return typeIdx;
		}
		return UINT32_MAX;
	}

	bool createBuffer(const DeviceFunctions &deviceFunctions, VkDevice device,
		const VkPhysicalDeviceMemoryProperties &memoryProperties,
		VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memoryFlags, VkBuffer &buffer,
		VkDeviceMemory &memory)
	{
		///////////////////
		// Create buffer //
		///////////////////

		// Create a buffer with the specified size and usage flags
		// The buffer will be used for vertex data, index data, uniform data, etc., depending on the usage flags provided
		VkBufferCreateInfo bufferInfo = VkBufferCreateInfo();
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if(deviceFunctions.vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists
		}

		//////////////////////////
		// Memory configuration //
		//////////////////////////

		// Get the size, alignment, and memory type requirements for the buffer
		VkMemoryRequirements requirements = VkMemoryRequirements();
		deviceFunctions.vkGetBufferMemoryRequirements(device, buffer, &requirements);

		// Find a memory type that is suitable for the vertex buffer
		std::uint32_t memoryType = findMemoryType(memoryProperties, requirements.memoryTypeBits, memoryFlags);
		// If this is UINT32_MAX, then somehow there was no suitable memory type found. AFAICT this should never happen unless we ask for a memory flags that are not supported by the device
		if(memoryType == UINT32_MAX)
		{
			// Clear the buffer if we failed to find a suitable memory type
			deviceFunctions.vkDestroyBuffer(device, buffer, nullptr);
			buffer = VK_NULL_HANDLE;
			return false; // TODO: Log this once error handling system exists
		}

		// Configure memory allocation for the buffer
		VkMemoryAllocateInfo allocInfo = VkMemoryAllocateInfo();
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = requirements.size;
		allocInfo.memoryTypeIndex = memoryType;

		// Attempt to allocate memory for the buffer
		if(deviceFunctions.vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
		{
			// Clear the buffer if we failed to allocate memory
			deviceFunctions.vkDestroyBuffer(device, buffer, nullptr);
			buffer = VK_NULL_HANDLE;
			return false; // TODO: Log this once error handling system exists
		}

		// Bind the allocated memory to the buffer
		if(deviceFunctions.vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS)
		{
			// Clear the buffer and free the memory if we failed to bind the memory to the buffer
			deviceFunctions.vkFreeMemory(device, memory, nullptr);
			memory = VK_NULL_HANDLE;
			deviceFunctions.vkDestroyBuffer(device, buffer, nullptr);
			buffer = VK_NULL_HANDLE;
			return false; // TODO: Log this once error handling system exists
		}

		return true;
	}
}