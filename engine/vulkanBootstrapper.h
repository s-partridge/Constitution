#ifndef CGE_VULKAN_BOOTSTRAPPER_H
#define CGE_VULKAN_BOOTSTRAPPER_H

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
		/////////////////////////////////////////////////////////////////////////////
		// Global functions that are loaded before creating a Vulkan instance.

		// Initial lookup function required to load all other Vulkan functions.
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
		// Fetch the count and properties of the available instance layers. Layers can be used for debugging, validation, and other purposes.
		PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = nullptr;
		// Create an instance of the Vulkan API. This is the first function that must be called after loading the Vulkan library.
		PFN_vkCreateInstance vkCreateInstance = nullptr;

		/////////////////////////////////////////////////////////////////////////////
		// Instance-level functions that are loaded after creating a Vulkan instance.
		// Destroy the Vulkan instance and free associated resources.
		PFN_vkDestroyInstance vkDestroyInstance = nullptr;

		// Destroy a Vulkan surface created for a window. This is used to clean up the surface when it is no longer needed.
		PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;

		// Enumerate the physical devices (GPUs) available on the system that support Vulkan.
		PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
		// Retrieve properties of a physical device, including name, type, API and driver versions, vendor and device IDs, and limits.
		PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
		// List a physical device's queue families and their properties (draw, compute, copy).
		PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
		// Check whether a queue family can show images on a gien surface.
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
		// Retrieve the capabilities of a surface for a given physical device, including max image count, current extent, supported transforms.
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
		// Retrieve the supported pixel formats for a surface.
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;

		// Retrieve a function pointer for a device-level function. This is used to load device-level functions after creating a Vulkan device.
		PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
		// Create a logical device from a physical device. This is used to create a device that can be used for rendering and compute operations.
		PFN_vkCreateDevice vkCreateDevice = nullptr;
	};

	struct DeviceFunctions
	{
		// Device-level functions that are loaded after creating a Vulkan device.
		// Destroy the Vulkan device and free associated resources.
		PFN_vkDestroyDevice vkDestroyDevice = nullptr;
		// Retrieve a queue from the Vulkan device. Queues are used to submit commands for execution on the GPU.
		PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;

		// Create a swapchain for a surface
		PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
		// Destroy a swapchain and free associated resources
		PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;

		// Retrieve the images associated with a swapchain. These images are used for rendering and presentation to the surface.
		PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
		// Create an image view for a swapchain image. These describe how the image will be accessed and used in shaders.
		PFN_vkCreateImageView vkCreateImageView = nullptr;
		// Destroy an image view and free associated resources.
		PFN_vkDestroyImageView vkDestroyImageView = nullptr;

		// Create a command pool for allocating command buffers. Command pools manage the memory used for command buffers.
		PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
		// Destroy a command pool and free associated resources.
		PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
		// Allocate command buffers from a command pool. Command buffers are used to record rendering and compute commands.
		PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
	};

	/**
	* Load global Vulkan functions that are required before creating a Vulkan instance.
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
	* Load instance-level Vulkan functions that are required after creating a Vulkan instance.
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
	* Load device-level Vulkan functions that are required after creating a Vulkan device.
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
}

#endif // CGE_VULKAN_BOOTSTRAPPER_HD
