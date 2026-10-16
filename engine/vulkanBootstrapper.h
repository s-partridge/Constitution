#ifndef CGE_VULKAN_BOOTSTRAPPER_H
#define CGE_VULKAN_BOOTSTRAPPER_H
#include <cstdint>

// Include this header before anything else that includes vulkan.h.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

// Before any Vulkan function can be used, its address needs to be acquired from the Vulkan loader.
// Linking against the SDK's import library would allow the OS to fill in the function pointers at load time, but it would also force a build dependency on the Vulkan SDK itself.
// On a system without Vulkan present at all, it would also prevent the application from starting, even if Vulkan is not used. This is not ideal for a game engine that may support multiple graphics APIs.
// Instead, we can load the Vulkan functions at runtime using vkGetInstanceProcAddr and vkGetDeviceProcAddr. This allows the application to start even if Vulkan is not present, and it also allows for more flexibility in loading only the functions that are needed for a specific use case.
// 
// This header performs the same function as third-party libraries like Volk, but it only exposes functions needed by CGE.
namespace cge::vulkan
{
	// TODO: Implement initial internal call directly at the OS level so no hand-off is necessary.
	// Requires OS-specific code for Windows, Linux, and MacOS to fetch vkGetInstanceProcAddr from the Vulkan loader library.
	// This would remove the need for GLFW or other libraries to provide this function pointer.
	struct VulkanFunctions
	{
		///////////////////////////////////////////////////////////////
		// Global functions loaded before creating a Vulkan instance //
		///////////////////////////////////////////////////////////////

		/**
		 * Return function pointer for command
		 * This must be set before any other Vulkan function can be called
		 * @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetInstanceProcAddr.html
		 */
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
		/**
		* Enumerate available Vulkan instance layers on the system
		* Used to check for validation layers and other optional features
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumerateInstanceLayerProperties.html
		*/
		PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = nullptr;
		/**
		* Create Vulkan instance
		* This is the first function that must be called after loading the Vulkan library.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateInstance.html
		*/
		PFN_vkCreateInstance vkCreateInstance = nullptr;

		/////////////////////////////////////////////////////////////////////
		// Instance-level function loaded after creating a Vulkan instance //
		/////////////////////////////////////////////////////////////////////
		/**
		* Destroy Vulkan instance and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyInstance.html
		*/
		PFN_vkDestroyInstance vkDestroyInstance = nullptr;

		/**
		* Destroy Vulkan surface created for a window
		* Used to clean up the surface when it is no longer needed.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroySurfaceKHR.html
		*/
		PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;

		/**
		* Enumerate physical devices (GPUs) available on the system that support Vulkan
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumeratePhysicalDevices.html
		*/
		PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
		/**
		* Retrieve properties of a physical device
		* Includes name, type, API and driver versions, vendor and device IDs, and limits.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceProperties.html
		*/
		PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
		/**
		* List physical device's queue families and their properties (draw, compute, copy)
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceQueueFamilyProperties.html
		*/
		PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
		/**
		* Check whether a queue family can show images on a given surface
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceSurfaceSupportKHR.html
		*/
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
		/**
		* Retrieve capabilities of surface for a given physical device
		* Includes max image count, current extent, supported transforms, etc.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceSurfaceCapabilitiesKHR.html
		*/
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
		/**
		* Retrieve supported pixel formats for a surface
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceSurfaceFormatsKHR.html
		*/
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;

		/**
		* Retrieve function pointer for a device-level function
		* Required to load device-level functions after creating a Vulkan device.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceProcAddr.html
		*/
		PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
		/**
		* Create logical device from a physical device
		* Used to create a device that can be used for rendering and compute operations
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateDevice.html
		*/
		PFN_vkCreateDevice vkCreateDevice = nullptr;

		/**
		* Retrieve memory properties of a physical device
		* Used to determine the types of memory available for buffers and images, and their properties.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceMemoryProperties.html
		*/
		PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
	};

	struct DeviceFunctions
	{
		//////////////////////
		// Device functions //
		//////////////////////
		
		/** 
		* Destroy the Vulkan device and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyDevice.html
		*/
		PFN_vkDestroyDevice vkDestroyDevice = nullptr;
		/**
		* Retrieve a queue from the Vulkan device
		* Queues are used to submit commands for execution on the GPU
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceQueue.html
		*/
		PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;

		/////////////////////////
		// Swapchain functions //
		/////////////////////////
		/**
		* Create a swapchain for a surface
		* Swap chains are used to manage multiple images that are presented to a surface in a controlled manner.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateSwapchainKHR.html
		*/
		PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
		/**
		* Destroy a swapchain and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroySwapchainKHR.html
		*/
		PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
		/**
		* Retrieve the images associated with a swapchain
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetSwapchainImagesKHR.html
		*/
		PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
		/**
		* Get index of the next available swapchain image for rendering.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImageKHR.html
		*/
		PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;

		/////////////////////////
		// ImageView functions //
		/////////////////////////
		/**
		* Create image view for a swapchain image
		* These describe how an image will be accessed and used in shaders
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateImageView.html
		*/
		PFN_vkCreateImageView vkCreateImageView = nullptr;
		/**
		* Destroy image view and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyImageView.html
		*/
		PFN_vkDestroyImageView vkDestroyImageView = nullptr;

		////////////////////////////
		// Command pool functions //
		////////////////////////////
		/**
		* Create command pool
		* Command pools manage the memory used for command buffers
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateCommandPool.html
		*/
		PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
		/**
		* Destroy command pool and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyCommandPool.html
		*/
		PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;

		//////////////////////////////
		//Synchronization functions //
		//////////////////////////////

		PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
		PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
		PFN_vkCreateFence vkCreateFence = nullptr;
		PFN_vkDestroyFence vkDestroyFence = nullptr;

		/////////////////////
		// Fence functions //
		/////////////////////
		/**
		* Wait for one or more fences owned by a device to become signaled
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitForFences.html
		*/
		PFN_vkWaitForFences vkWaitForFences = nullptr;
		/**
		* Reset one or more fences to the unsignaled state, allowing them to be reused for synchronization.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkResetFences.html
		*/
		PFN_vkResetFences vkResetFences = nullptr;

		//////////////////////////////
		// Command buffer functions //
		//////////////////////////////
		/**
		* Allocate command buffers from a command pool
		* Command buffers are used to record rendering and compute commands.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkAllocateCommandBuffers.html
		*/
		PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
		/**
		* Start recording commands to a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkBeginCommandBuffer.html
		*/
		PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
		/**
		* Finish recording commands to a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkEndCommandBuffer.html
		*/
		PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
		/**
		* Copy data from one buffer to another
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBuffer.html
		*/
		PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
		
		////////////////////////
		// Pipeline functions //
		////////////////////////
		/**
		* Insert pipeline barrier into command buffer to synchronize access to resources between different stages of the pipeline
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPipelineBarrier2.html
		*/
		PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2 = nullptr;
		/**
		* Begin rendering pass in a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBeginRendering.html
		*/
		PFN_vkCmdBeginRendering vkCmdBeginRendering = nullptr;
		/**
		* Bind a graphics or compute pipeline to a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindPipeline.html
		*/
		PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
		/**
		* Set the viewport for a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetViewport.html
		*/
		PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
		/**
		* Set the scissor rectangle for a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetScissor.html
		*/
		PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
		/**
		* Bind vertex buffers to a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindVertexBuffers.html
		*/
		PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
		/**
		* Bind index buffer to a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindIndexBuffer.html
		*/
		PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
		/**
		* Push constants to a command buffer
		* Push constants are small pieces of data that can be passed directly to shaders without the need for a buffer or descriptor set.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPushConstants.html
		*/
		PFN_vkCmdPushConstants vkCmdPushConstants = nullptr;
		/**
		* Issue a draw call using indexed geometry
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDrawIndexed.html
		*/
		PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;

		/**
		* End rendering pass in a command buffer
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdEndRendering.html
		*/
		PFN_vkCmdEndRendering vkCmdEndRendering = nullptr;

		/**
		* Submit one or more command buffers to a queue for execution on the GPU
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit2.html
		*/
		PFN_vkQueueSubmit2 vkQueueSubmit2 = nullptr;
		/**
		* Queue image for presentation
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html
		*/
		PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;

		////////////////////
		// Wait functions //
		////////////////////
		/**
		* Wait for a queue to finish all pending operations
		* This is used to ensure that all commands submitted to the queue have completed
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueWaitIdle.html
		*/
		PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
		/**
		* Wait for the device to finish all pending operations
		* This is used to ensure that all commands have completed before cleaning up resources
		* or exiting the application.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDeviceWaitIdle.html
		*/
		PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;

		//////////////////////
		// Shader functions //
		//////////////////////
		
		/**
		* Create shader module from SPIR-V bytecode
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateShaderModule.html
		*/
		PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
		/**
		* Destroy shader module and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyShaderModule.html
		*/
		PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;

		/**
		* Create pipeline layout
		* A pipeline layout defines the interface between shader stages and shader resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreatePipelineLayout.html
		*/
		PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
		/**
		* Destroy pipeline layout and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyPipelineLayout.html
		*/
		PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
		/**
		* Create one or more graphics pipelines
		* Graphics pipelines define the stages of the graphics pipeline (vertex shader, fragment shader, etc.) and how they are connected.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateGraphicsPipelines.html
		*/
		PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
		/**
		* Destroy graphics pipeline and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyPipeline.html
		*/
		PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;

		//////////////////////
		// Memory functions //
		//////////////////////
		/**
		* Create buffer object for storing vertex data, index data, uniform data, etc.
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateBuffer.html
		*/
		PFN_vkCreateBuffer vkCreateBuffer = nullptr;
		/**
		* Destroy buffer object and free associated resources
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyBuffer.html
		*/
		PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;

		/**
		* Retrieve memory requirements for a buffer object
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkGetBufferMemoryRequirements.html
		*/
		PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;

		/**
		* Allocate device memory for buffers and images
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkAllocateMemory.html
		*/
		PFN_vkAllocateMemory vkAllocateMemory = nullptr;
		/**
		* Free device memory previously allocated with vkAllocateMemory
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkFreeMemory.html
		*/
		PFN_vkFreeMemory vkFreeMemory = nullptr;
		/**
		* Bind a buffer to a specific memory allocation
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkBindBufferMemory.html
		*/
		PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;

		/**
		* Map a memory object into the application's address space
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkMapMemory.html
		*/
		PFN_vkMapMemory vkMapMemory = nullptr;
		/**
		* Unmap a previously mapped memory object
		* @see https://docs.vulkan.org/refpages/latest/refpages/source/vkUnmapMemory.html
		*/
		PFN_vkUnmapMemory vkUnmapMemory = nullptr;
	};

	/**
	* Load global Vulkan functions that are required before creating a Vulkan instance
	* 
	* @param instanceFunctions A reference to an InstanceFunctions struct where the loaded function pointers will be stored.
	* @param getInstanceProcAddr A function pointer to vkGetInstanceProcAddr, which must be fetched from the Vulkan loader library (e.g., via GLFW or OS-specific methods).
	* 
	* @return true if all required global functions were successfully loaded; false otherwise.
	* 
	* @note getInstanceProcAddr must be a valid function pointer to vkGetInstanceProcAddr before calling this function.
	*/
	bool loadGlobalfunctions(VulkanFunctions &instanceFunctions, PFN_vkGetInstanceProcAddr getInstanceProcAddr);
	/**
	* Load instance-level Vulkan functions that are required after creating a Vulkan instance
	* 
	* @param instanceFunctions A reference to an InstanceFunctions struct where the loaded function pointers will be stored.
	* @param instance The Vulkan instance from which to load the functions.
	*
	* @return true if all required instance functions were successfully loaded; false otherwise.
	* 
	* @note instance must be a valid VkInstance that has been created using vkCreateInstance before calling this function.
	*/
	bool loadInstanceFunctions(VulkanFunctions &instanceFunctions, VkInstance instance);
	/**
	* Load device-level Vulkan functions that are required after creating a Vulkan device
	* 
	* @param deviceFunctions A reference to a DeviceFunctions struct where the loaded function pointers will be stored.
	* @param instanceFunctions A reference to an InstanceFunctions struct where instance-level function pointers are stored. Must already be loaded before calling this function.
	* @param device The Vulkan device from which to load the functions.
	*
	* @return true if all required device functions were successfully loaded; false otherwise.
	* 
	* @note device must be a valid VkDevice that has been created using vkCreateDevice before calling this function. instanceFunctions must have been populated with valid function pointers from loadInstanceFunctions.
	*/
	bool loadDeviceFunctions(DeviceFunctions &deviceFunctions, VulkanFunctions &instanceFunctions, VkDevice device);

	/**
	* Fetch the index of a memory type that satisfies the requested properties and is compatible with the allowed type bits
	* 
	* @param memoryProperties The memory properties of the physical device, obtained from vkGetPhysicalDeviceMemoryProperties()
	* @param allowedTypeBits A bitmask of allowed memory types, typically obtained from VkMemoryRequirements::memoryTypeBits
	* @param wantedFlags The desired memory property flags, such as VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	* 
	* @return The index of a suitable memory type
	*/
	std::uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProperties, std::uint32_t allowedTypeBits, VkMemoryPropertyFlags wantedFlags);

	bool createBuffer(const DeviceFunctions &deviceFunctions, VkDevice device,
		const VkPhysicalDeviceMemoryProperties &memoryProperties,
		VkDeviceSize size,  VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags memoryFlags, VkBuffer &buffer,
		VkDeviceMemory &memory);
}

#endif // CGE_VULKAN_BOOTSTRAPPER_HD
 