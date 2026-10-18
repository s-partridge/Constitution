#include <cstddef> // for std::uint32_t
#include <cstring> // for memcpy

#include "windowSystem.h"
#include "event.h"
#include "engine.h"
#include "resourceLoader.h"
#include "renderPrimitives.h"

#include "vulkanBootstrapper.h"
#define GLFW_INCLUDE_VULKAN	// Using Vulkan instead of OpenGL.
#include <GLFW/glfw3.h>

namespace cge
{
	WindowSystem::WindowSystem(const std::string &name, const std::string &windowTitle)
		: SystemBase(name), m_windowSystemBroadcaster(Engine::Instance().getDispatcher()),
		  m_windowTitle(windowTitle)
	{
		m_shutdownChannel = &Engine::Instance().getEventRegistry().getChannel<bool>(EngineEvent::EngineShutdown);
	}

	bool WindowSystem::setUpVulkan()
	{
		////////////////////////////////////////////////////////////////
		// Initialize core Vulkan functions and create a Vulkan instance
		////////////////////////////////////////////////////////////////

		// Create a new VulkanFunctions object to hold the function pointers for Vulkan functions.
		m_vulkan = new vulkan::VulkanFunctions();
		// Get the initial function pointer for vkGetInstanceProcAddr from GLFW. This function is used to load other Vulkan functions.
		PFN_vkGetInstanceProcAddr vulkanLocator = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
		// Pass it to the Vulkan bootstrapper to load the global functions. If it fails, we log and return.
		if(vulkanLocator == nullptr || !vulkan::loadGlobalfunctions(*m_vulkan, vulkanLocator))
			// Nothing to do here; the engine will fail to start if Vulkan is not available. Just log this and return.
			// TODO: Log this once error handling system exists.
			return false;

		// Get the required Vulkan instance extensions from GLFW. These are necessary for creating a Vulkan instance that can interface with the window system.
		std::uint32_t instanceExtensionCount = 0;
		// The extensions should be VK_KHR_surface and a platform-specific extension like VK_KHR_win32_surface, required for drawing to a window.
		const char **extensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCount);
		// A nullptr here would mean that Vulkan is present, but is not able to interface with the window system. This could happen, for example, on a server without a display. In that case, we should log and return.
		// The window system likely shouldn't be loaded at all in those configurations, but we should still handle it gracefully.
		if(extensions == nullptr)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		// Create a Vulkan application info structure. This is used to provide information about the application to the Vulkan driver.
		VkApplicationInfo appInfo = VkApplicationInfo();
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = m_windowTitle.c_str();
		appInfo.pEngineName = "Constitution";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Verion number for this application. Can be anything, should match the version of the game.
		appInfo.apiVersion = VK_API_VERSION_1_4;

		// Create a Vulkan "InstanceCreateInfo" structure. This is used to specify the parameters for creating a Vulkan instance.
		VkInstanceCreateInfo instanceInfo = VkInstanceCreateInfo();
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo = &appInfo;
		instanceInfo.enabledExtensionCount = instanceExtensionCount;
		instanceInfo.ppEnabledExtensionNames = extensions;

		/////////////////////////////////////////
		// Vulkan Debugging and Validation Layers
		/////////////////////////////////////////
#ifdef _DEBUG
		// Enable validation layers in debug builds, if available.
		// VK_LAYER_KHRONOS_validation is only available when the Vulkan SDK is present on the system.
		// Validation layers are used to check for errors and provide debugging information.

		// Get the count of available layers
		std::uint32_t layerCount = 0;
		m_vulkan->vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		// Run the same enumeration with a non-null pointer to get the actual layer properties. This is used to check if the validation layer is available.
		VkLayerProperties *availableLayers = new VkLayerProperties[layerCount];
		m_vulkan->vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

		const char *validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
		bool hasValidationLayer = false;

		#pragma warning (push)
		#pragma warning (disable : 6385) // Disable warning about reading out of bounds. The size won't grow between these two calls.
		for(std::uint32_t i = 0; i < layerCount; ++i)
		{
			if(std::strcmp(availableLayers[i].layerName, validationLayers[0]) == 0)
			{
				hasValidationLayer = true;
				break;
			}
		}
		#pragma warning (pop)

		delete[] availableLayers;

		if(hasValidationLayer)
		{
			instanceInfo.enabledLayerCount = 1;
			instanceInfo.ppEnabledLayerNames = validationLayers;
		}
		// TODO: Else, record debug-level log that the system does not have the validation layer available.
		// TODO: Add a debug messenger (VK_EXT_debug_utils) once a renderer system exists, to route validation messages to the engine's log.
		// Until then, the validation layer prints its own messages to the console and, on Windows, the debugger output.
#endif
		/////////////////////////////////////////////////////////////////////////////
		// Create vulkan instance, load instance-level functions, create GLFW surface
		/////////////////////////////////////////////////////////////////////////////

		// Create the Vulkan instance using the configuration specified in the createInfo structure.
		if(m_vulkan->vkCreateInstance(&instanceInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
		{
			// TODO: Log this once error handling system exists.
			return false;
		}

		// Load the instance-level Vulkan functions using the created Vulkan instance. This allows us to use Vulkan functions that require a valid instance.
		loadInstanceFunctions(*m_vulkan, m_vkInstance);

		// Create a Vulkan surface for the GLFW window. This surface is used to present rendered images to the window.
		if(glfwCreateWindowSurface(m_vkInstance, m_window, nullptr, &m_surface) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		///////////////////////////////////////////////////
		// Select a Vulkan physical device and queue family
		///////////////////////////////////////////////////

		std::uint32_t deviceCount = 0;
		m_vulkan->vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
		// Make sure there actually is a Vulkan-capable GPU available on the system.
		if(deviceCount == 0)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		#pragma warning (push)
		#pragma warning (disable : 6385) // Disable warning about reading out of bounds. The size won't grow between these two calls.
		// List the available physical devices (GPUs) that support Vulkan.
		VkPhysicalDevice *devices = new VkPhysicalDevice[deviceCount];
		m_vulkan->vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices);
		for(std::uint32_t deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx)
		{
			std::uint32_t queueFamilyCount = 0;
			m_vulkan->vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIdx], &queueFamilyCount, nullptr);

			VkQueueFamilyProperties *queueFamilies = new VkQueueFamilyProperties[queueFamilyCount];
			m_vulkan->vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIdx], &queueFamilyCount, queueFamilies);

			// Find a queue family that can draw and show images on the surface.
			for(std::uint32_t queueIdx = 0; queueIdx < queueFamilyCount; ++queueIdx)
			{
				VkBool32 canPresent = VK_FALSE;
				m_vulkan->vkGetPhysicalDeviceSurfaceSupportKHR(devices[deviceIdx], queueIdx, m_surface, &canPresent);

				// Check that the family supports graphics operations and can also present to a surface.
				if((queueFamilies[queueIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && canPresent == VK_TRUE)
				{
					m_physicalDevice = devices[deviceIdx];
					m_activeQueueFamilyIndex = queueIdx;
					break;
				}
			}
			delete[] queueFamilies;
			if(m_physicalDevice != nullptr)
				break;
		}
		#pragma warning (pop)
		delete[] devices;

		if(m_physicalDevice == nullptr)
		{
			return false; // TODO: Log this once error handling system exists.
		}
		
		/////////////////////////////////////////
		// Create Vulkan logical device and queue
		/////////////////////////////////////////

		// Create a queue from the family and physical device selected.
		// Priority is a float between 0.0 and 1.0, where 1.0 is the highest priority. This is used to determine how the GPU schedules work across multiple queues.
		// For now, with one queue, that doesn't matter.
		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueInfo = VkDeviceQueueCreateInfo();
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = m_activeQueueFamilyIndex;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &queuePriority;

		// Enable dynamic rendering and synchronization features for the logical device.
		// TODO: Check whether these features are actually supported by the physical device before enabling them. If not, we should log and return. This is useful if there are multiple GPUs with different capabilities on the system and a fallback can be selected.
		// Required features can be checked with vkGetPhysicalDeviceFeatures2 and the appropriate feature structure. Feature structures can be chained together with the pNext pointer.
		//   This should be handled by device creation returning VK_ERROR_FEATURE_NOT_PRESENT
		VkPhysicalDeviceVulkan13Features deviceFeatures = VkPhysicalDeviceVulkan13Features();
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		deviceFeatures.dynamicRendering = VK_TRUE;
		deviceFeatures.synchronization2 = VK_TRUE;

		const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo deviceInfo = VkDeviceCreateInfo();
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pNext = &deviceFeatures; // pNext is used to chain additional structures to the create info. It functions as a linked list. TODO: Figure out how else this is used
		deviceInfo.queueCreateInfoCount = 1;
		deviceInfo.pQueueCreateInfos = &queueInfo;
		deviceInfo.enabledExtensionCount = 1;
		deviceInfo.ppEnabledExtensionNames = deviceExtensions;

		// Create the logical device along with its associated queue
		if(m_vulkan->vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		m_deviceFunctions = new vulkan::DeviceFunctions();
		vulkan::loadDeviceFunctions(*m_deviceFunctions, *m_vulkan, m_device);

		// Fetch the queue from the logical device
		m_deviceFunctions->vkGetDeviceQueue(m_device, m_activeQueueFamilyIndex, 0, &m_queue);

		////////////////////////////
		// Create a Vulkan swapchain
		////////////////////////////

		// Check surface capabilities to determine the supported formats, present modes, etc.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = VkSurfaceCapabilitiesKHR();
		m_vulkan->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

		// Choose a pixel format.
		std::uint32_t formatCount = 0;
		m_vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
		VkSurfaceFormatKHR *surfaceFormats = new VkSurfaceFormatKHR[formatCount];
		m_vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, surfaceFormats);

		// Default to the first format in the list, but prefer VK_FORMAT_B8G8R8A8_SRGB with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR if available.
		// This format is standard for sRGB color space on desktops and is widely supported.
		VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
		for(std::uint32_t formatIdx = 0; formatIdx < formatCount; ++formatIdx)
		{
			if(surfaceFormats[formatIdx].format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormats[formatIdx].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = surfaceFormats[formatIdx];
				break;
			}
		}
		delete[] surfaceFormats;

		VkExtent2D swapchainExtent = surfaceCapabilities.currentExtent;
		if(swapchainExtent.width == UINT32_MAX || swapchainExtent.height == UINT32_MAX)
		{
			// If the surface size is undefined, we can set it to our desired size.
			int width = 0;
			int height = 0;
			glfwGetFramebufferSize(m_window, &width, &height);
			swapchainExtent.width = (std::uint32_t)width;
			swapchainExtent.height = (std::uint32_t)height;
		}

		// Ensure there is at least one more image than the minimum required by the surface. This allows for buffering.
		std::uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if(surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		{
			imageCount = surfaceCapabilities.maxImageCount;
		}
		VkSwapchainCreateInfoKHR swapchainInfo = VkSwapchainCreateInfoKHR();
		swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainInfo.surface = m_surface;
		swapchainInfo.minImageCount = imageCount;
		swapchainInfo.imageFormat = surfaceFormat.format;
		swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapchainInfo.imageExtent = swapchainExtent;
		swapchainInfo.imageArrayLayers = 1; // 1 for normal 2D images, 2 for stereoscopic 3D images.
		swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Tells Vulkan that we will draw to these images.
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Images are owned by one queue family at a time.
		swapchainInfo.preTransform = surfaceCapabilities.currentTransform; // No transformation is applied to the images. This keeps whatever the surface is currently doing.
		swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Don't use the alpha channel. Makes the entire window opaque. This is standard for most applications.
		swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Display frames in order, one per vertical refresh. This is the only mode guaranteed to be available on all platforms.
		swapchainInfo.clipped = VK_TRUE; // Don't care about pixels that are obscured by other windows. This allows the GPU to optimize rendering. On Windows, this won't matter for screen recording, but apparently that's because of how Windows composites windows. And also apparently this can't be flipped during runtime, so the context would need to be recreated if the user wants to change it.

		if(m_deviceFunctions->vkCreateSwapchainKHR(m_device, &swapchainInfo, nullptr, &m_swapchain) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		// Store the swapchain format and dimensions for later use.
		m_swapchainFormat = (uint32_t)surfaceFormat.format;
		m_swapchainWidth = swapchainExtent.width;
		m_swapchainHeight = swapchainExtent.height;

		///////////////////////////////////////////////////////////
		// Create and retrieve the swapchain image views and images
		///////////////////////////////////////////////////////////

		m_deviceFunctions->vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, nullptr);
		m_swapchainImages = new VkImage[m_swapchainImageCount];
		m_deviceFunctions->vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, m_swapchainImages);

		// Create an image view for each swapchain image. Image views describe how the images will be accessed and used in shaders.
		// The array is value-initialized to nullptr so that if part of the array fails to initialize it can still be destroyed smoothly.
		m_swapchainImageViews = new VkImageView[m_swapchainImageCount]();

		for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
		{
			VkImageViewCreateInfo viewInfo = VkImageViewCreateInfo();
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_swapchainImages[imageIdx];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = surfaceFormat.format;
			// subresourceRange declares what part of an image the view is for. It can be configure for mipmaps, array layers, and aspects (color, depth, stencil).
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			//view.components.r also exists, along with g, b, and a, for remapping with VK_COMPONENT_SWIZZLE_* values.
			
			if(m_deviceFunctions->vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[imageIdx]) != VK_SUCCESS)
			{
				return false; // TODO: Log this once error handling system exists.
			}
		}

		////////////////////////////////////////
		// Create Vulkan command pool and buffer
		////////////////////////////////////////

		VkCommandPoolCreateInfo poolInfo = VkCommandPoolCreateInfo();
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		// Allow each buffer to be individually reset. This is useful for re-recording command buffers without having to reset the entire pool.
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_activeQueueFamilyIndex;

		// Create the command pool
		if(m_deviceFunctions->vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		VkCommandBufferAllocateInfo bufferInfo = VkCommandBufferAllocateInfo();
		bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufferInfo.commandPool = m_commandPool;
		bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary buffers can be submitted to a queue, while secondary buffers can only be called from primary buffers.
		bufferInfo.commandBufferCount = 1;

		// Allocate the command buffer
		if(m_deviceFunctions->vkAllocateCommandBuffers(m_device, &bufferInfo, &m_commandBuffer) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		/////////////////////////////////////
		// Create Vulkan semaphores and fence
		/////////////////////////////////////

		VkSemaphoreCreateInfo semaphoreInfo = VkSemaphoreCreateInfo();
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if(m_deviceFunctions->vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailable) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		// Create one semaphore for each swapchain image to signal when rendering is finished.
		m_renderFinished = new VkSemaphore[m_swapchainImageCount]();
		for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
		{
			if(m_deviceFunctions->vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinished[imageIdx]) != VK_SUCCESS)
			{
				return false; // TODO: Log this once error handling system exists.
			}
		}

		// Create a fence to synchronize CPU and GPU work.
		VkFenceCreateInfo fenceInfo = VkFenceCreateInfo();
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		// Create the fence in a signaled state so that the first frame can be rendered without waiting.
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if(m_deviceFunctions->vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFence) != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		return true;
	}

	bool WindowSystem::setUpShaders()
	{
		/////////////////////
		// Shader creation //
		/////////////////////
		VkShaderModule vertexShader = loadVulkanShaderModule(*m_deviceFunctions, m_device, "../engine/shaders/vertexColor.vert.spv");
		if(vertexShader == nullptr)
		{
			return false;
		}

		VkShaderModule fragmentShader = loadVulkanShaderModule(*m_deviceFunctions, m_device, "../engine/shaders/fragmentColor.frag.spv");

		// VkShaderModule is an owning handle, so if the fragment shader fails to load, we need to destroy the vertex shader before returning.
		if(fragmentShader == nullptr)
		{
			m_deviceFunctions->vkDestroyShaderModule(m_device, vertexShader, nullptr);
			return false; // TODO: Log this once error handling system exists.
		}

		/////////////////////////////
		// Crerate pipeline layout //
		/////////////////////////////
		
		// For vertexColor.vert we have aa single constant for the MVC transform matrix
		// We have to specify it by exact size and offset within the shader's push constant block, and the shader stage that will use it
		VkPushConstantRange pushConstantRange = VkPushConstantRange();
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = 64;	// A 4x4 matrix of floats, 16 * 4 bytes = 64 bytes.

		// Create the pipeline layout, which specifies the resources that will be available to the shaders.
		VkPipelineLayoutCreateInfo layoutInfo = VkPipelineLayoutCreateInfo();
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstantRange;

		if(m_deviceFunctions->vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		{
			m_deviceFunctions->vkDestroyShaderModule(m_device, vertexShader, nullptr);
			m_deviceFunctions->vkDestroyShaderModule(m_device, fragmentShader, nullptr);
			return false; // TODO: Log this once error handling system exists.
		}

		//////////////////////////
		// Create shader stages //
		//////////////////////////
		VkPipelineShaderStageCreateInfo stages[2];
		// Vertex shader
		stages[0] = VkPipelineShaderStageCreateInfo();
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertexShader;
		stages[0].pName = "main"; // Entry point of the shader. This is the function that will be called when the shader is executed.

		// Fragment shader
		stages[1] = VkPipelineShaderStageCreateInfo();
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fragmentShader;
		stages[1].pName = "main"; // Entry point of the shader. This is the function that will be called when the shader is executed.

		//////////////////////////
		// Vertex shader layout //
		//////////////////////////

		// Binding describes how the vertex data is laid out in memory. It specifies the stride (size of each vertex) and the input rate (per-vertex or per-instance).
		VkVertexInputBindingDescription vertexBinding = VkVertexInputBindingDescription();
		vertexBinding.binding = 0; // Binding index for the vertex buffer
		vertexBinding.stride = sizeof(primitives::Vertex); // Size of each vertex in bytes
		vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Move to the next vertex for each vertex shader invocation. This can also be per-instance. I don't know what "instance" means. Whole entire mesh? Contiguous surface? The documentation doesn't seem to specify

		// Per-vertex attributes that will be passed to the vertex shader
		VkVertexInputAttributeDescription vertexAttributes[2];
		vertexAttributes[0] = VkVertexInputAttributeDescription();
		vertexAttributes[0].location = 0; // Location of the attribute in the shader. This should match the location specified in the shader.
		vertexAttributes[0].binding = 0; // Binding index for the vertex buffer.
		vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Format of the attribute. This should match the type of the attribute in the shader. This is used both for colors (rgb) and positions (xyz) if their source type is a three-component float vector.
		vertexAttributes[0].offset = offsetof(primitives::Vertex, position); // Offset of the attribute in the vertex struct.

		vertexAttributes[1] = VkVertexInputAttributeDescription();
		vertexAttributes[1].location = 1; // Location of the attribute in the shader. This should match the location specified in the shader.
		vertexAttributes[1].binding = 0; // Binding index for the vertex buffer.
		vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT; // Format of the attribute. This should match the type of the attribute in the shader.
		vertexAttributes[1].offset = offsetof(primitives::Vertex, color); // Offset of the attribute in the vertex struct.

		// Link the binding and attribute descriptions to the pipeline input state for the vertex shader
		// This explicitly tells Vulkan to expect a vertex buffer with the specified layout, and how to interpret the data in that buffer
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = VkPipelineVertexInputStateCreateInfo();
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
		vertexInputInfo.vertexAttributeDescriptionCount = 2;
		vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes;

		///////////////////////////
		// Fixed pipeline inputs //
		///////////////////////////

		// Input assembly describes how the vertices will be assembled into primitives. In this case, we are using a triangle list, which means that every three vertices will form a triangle. This, as opposed to a triangle strip or fan.
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkPipelineInputAssemblyStateCreateInfo();
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		
		// Viewport state defines the viewport and scissor rectangles. These determine the region of the framebuffer that the output will be rendered to.
		// The viewport maps  the image space onto real pixels on the screen, while the scissor rectangle defines the region of the framebuffer that will be affected by rendering. Anything outside the scissor rectangle will be discarded.
		VkPipelineViewportStateCreateInfo viewportState = VkPipelineViewportStateCreateInfo();
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1; // One viewport
		viewportState.scissorCount = 1; // One scissor rectangle

		// Settings for the rasterization pipeline
		// Rasterization is the process of converting the vertex data into fragments (pixels) that will be drawn to the framebuffer. This includes culling, depth testing, and other operations.
		VkPipelineRasterizationStateCreateInfo rasterizationInfo = VkPipelineRasterizationStateCreateInfo();
		rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT; // Cull backfaces
		rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Normal winding order is counter-clockwise
		rasterizationInfo.lineWidth = 1.0f; // This must be 1.0 unless wide lines are enabled, which is not supported on all hardware. non-one values are actually fine when drawing in fill mode but will produce a warning in the validation layer.

		// For antialiasing, which for now is disabled
		VkPipelineMultisampleStateCreateInfo multisampleInfo = VkPipelineMultisampleStateCreateInfo();
		multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // No multisampling

		// Set which color channels will be written to the framebuffer and whether blending is enabled. Blending is used for transparency and other effects
		// There must be one colorBlendAttachment for each output image, which will be declared in the pipeline rendering state
		VkPipelineColorBlendAttachmentState colorBlendAttachment = VkPipelineColorBlendAttachmentState();
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE; // No blending

		// Create the color blend state, which specifies how the color blending will be applied to the framebuffer
		// AttachmentCount must match the number of color attachments in the pipeline rendering state
		VkPipelineColorBlendStateCreateInfo colorBlendInfo = VkPipelineColorBlendStateCreateInfo();
		colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendInfo.attachmentCount = 1;
		colorBlendInfo.pAttachments = &colorBlendAttachment;

		///////////////////
		// Dynamic state //
		///////////////////
		// Dynamic state allows certain pipeline states to be changed without recreating the pipeline. This is useful for states that change frequently, like viewport and scissor.
		// Anything not listed here is static and cannot be changed without recreating the pipeline. This includes input assembly, rasterization, multisampling, and color blending.
		VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		// Create the dynamic state info, which will point to the array of dynamic states
		VkPipelineDynamicStateCreateInfo dynamicStateInfo = VkPipelineDynamicStateCreateInfo();
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = 2;
		dynamicStateInfo.pDynamicStates = dynamicStates;
		
		//////////////////
		// Image format //
		//////////////////

		// The format of the swapchain images, which will be used as the color attachment in the pipeline rendering state. The cast is necessary because VkFormat can't be forward-declared.
		//TODO: This must be fixed when the rendering code is extracted into its own class so no cast is necessary. Header-includers are avoided here so that game projects never need to link against Vulkan.
		VkFormat colorFormat = (VkFormat)m_swapchainFormat;

		VkPipelineRenderingCreateInfo renderFormat = VkPipelineRenderingCreateInfo();
		renderFormat.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		// Must match the value passed to vkCmdBeginRendering. This is the number of color attachments that will be used in the rendering pass.
		renderFormat.colorAttachmentCount = 1;
		// Must match the format of the swapchain images. This is the format that will be used for the color attachment in the rendering pass.
		renderFormat.pColorAttachmentFormats = &colorFormat;
		// If we need a depth buffer, we can add it here.
		//renderFormat.depthAttachmentFormat

		///////////////////////////
		// Pipeline construction //
		///////////////////////////
		
		VkGraphicsPipelineCreateInfo pipelineInfo = VkGraphicsPipelineCreateInfo();
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = &renderFormat; // Chain the rendering format info to the pipeline create info
		pipelineInfo.stageCount = 2;		// There are two shader stages for this pipeline
		pipelineInfo.pStages = stages;		// The array of shader stages
		pipelineInfo.pVertexInputState = &vertexInputInfo; // The vertex input state
		// Fixed pipeline states
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizationInfo;
		pipelineInfo.pMultisampleState = &multisampleInfo;
		pipelineInfo.pColorBlendState = &colorBlendInfo;
		pipelineInfo.pDynamicState = &dynamicStateInfo; // The dynamic state info
		pipelineInfo.layout = m_pipelineLayout; // The pipeline layout, not including per-vertex data. This is the layout of the push constants and descriptor sets.

		VkResult pipelineResult = m_deviceFunctions->vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline);

		// Clean up initialization resources. The shader modules are no longer needed after the pipeline is created, so they can be destroyed.
		m_deviceFunctions->vkDestroyShaderModule(m_device, vertexShader, nullptr);
		m_deviceFunctions->vkDestroyShaderModule(m_device, fragmentShader, nullptr);

		if(pipelineResult != VK_SUCCESS)
			return false;

		return true;
	}

	bool WindowSystem::setUpMeshBuffers()
	{
		///////////////////////
		// Preliminary setup //
		///////////////////////
		// These are static for the given device, so we can fetch them once and store them for later use
		// For now we're only using them here
		VkPhysicalDeviceMemoryProperties memoryProperties = VkPhysicalDeviceMemoryProperties();
		m_vulkan->vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

		// Placeholder struct standing in for actual obj data
		mesh::Mesh mesh = mesh::buildSimpleMesh(
			primitives::cubeFaces, 6,
			primitives::cubePositions, 8,
			primitives::colorPalette, 6
		);

		//////////////////////////////
		// Vertex and index buffers //
		//////////////////////////////

		// Logical size of the vertex and index buffers in bytes
		VkDeviceSize vertexBufferSize = mesh.vertexCount * sizeof(primitives::Vertex);
		VkDeviceSize indexBufferSize = mesh.indexCount * sizeof(std::uint32_t);

		// Configure the buffers to be used as vertex and index buffers, and to be transfer destinations for the staging buffer. This allows us to copy data from the staging buffer to the vertex and index buffers on the GPU.
		VkBufferUsageFlags vertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VkBufferUsageFlags indexUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		// Configure the backing memory to be device-local, as opposed to host-visible.
		// This means that the memory will be allocated on the GPU, which is faster for rendering, but cannot be accessed directly by the CPU.
		// We'll need to use a staging buffer to transfer data from the CPU to the GPU.
		VkMemoryPropertyFlags bufferFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		// Create the vertex buffer and allocate memory for it. This is a helper function that encapsulates the Vulkan buffer creation and memory allocation process.
		if(!vulkan::createBuffer(
			*m_deviceFunctions, m_device,
			memoryProperties, vertexBufferSize,
			vertexUsage, bufferFlags,
			m_vertexBuffer, m_vertexMemory))
		{
			return false; // TODO: Log this once error handling system exists.
		}

		if(!vulkan::createBuffer(
			*m_deviceFunctions, m_device,
			memoryProperties, indexBufferSize,
			indexUsage, bufferFlags,
			m_indexBuffer, m_indexMemory))
		{
			return false; // TODO: Log this once error handling system exists.
		}

		////////////////////
		// Staging buffer //
		////////////////////
		
		// The staging buffer will be used as a transfer source, to copy data to the vertex buffer.
		VkBufferUsageFlags stagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		// The staging buffer needs to be host-visible and coherent, so that the CPU can write to it directly and the GPU will see the changes without needing to flush or invalidate caches.
		VkMemoryPropertyFlags stagingMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		// Create local variables to hold the staging buffer and its memory
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		VkDeviceSize stagingBufferSize = vertexBufferSize + indexBufferSize;

		// Create the staging buffer and allocate memory for it. This is a helper function that encapsulates the Vulkan buffer creation and memory allocation process.
		if(!vulkan::createBuffer(
			*m_deviceFunctions, m_device,
			memoryProperties, stagingBufferSize,
			stagingUsage, stagingMemoryFlags,
			stagingBuffer, stagingMemory))
		{
			return false; // TODO: Log this once error handling system exists.
		}

		////////////////////
		// Memory mapping //
		////////////////////

		// Map the staging buffer memory to a CPU-accessible pointer. This allows us to write data to the buffer directly from the CPU.
		void *stagingData = nullptr;
		if(m_deviceFunctions->vkMapMemory(m_device, stagingMemory, 0, stagingBufferSize, 0, &stagingData) != VK_SUCCESS)
		{
			// Clear up the staging buffer and its memory before returning, since we failed to map the memory.
			m_deviceFunctions->vkFreeMemory(m_device, stagingMemory, nullptr);
			m_deviceFunctions->vkDestroyBuffer(m_device, stagingBuffer, nullptr);
			return false; // TODO: Log this once error handling system exists.
		}

		// Copy the vertex data from the mesh to the staging buffer
		// This puts the data in the staging buffer on the CPU side, which will then be transferred to the GPU-side vertex buffer in a later step
		std::memcpy(stagingData, mesh.vertices, static_cast<std::size_t>(vertexBufferSize));
		// And the index data, offset by the size of the vertex buffer
		std::memcpy(static_cast<char *>(stagingData) + vertexBufferSize, mesh.indices, static_cast<std::size_t>(indexBufferSize));

		// Unmap the staging buffer memory now that we're done writing to it
		// Since the buffer is going to be freed immediately after this, it isn't actually necessary to unmap it, but it's here for consistency.
		// Unmapping is primarily necessary when the buffer needs to be re-mapped later
		m_deviceFunctions->vkUnmapMemory(m_device, stagingMemory);

		///////////////////
		// Data transfer //
		///////////////////
		// Configure the command buffer to be used for the transfer operation
		VkCommandBufferBeginInfo copyBeginInfo = VkCommandBufferBeginInfo();
		copyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		copyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		m_deviceFunctions->vkBeginCommandBuffer(m_commandBuffer, &copyBeginInfo);

		// Configure the region of the buffer to copy. This specifies the source and destination offsets, and the size of the data to copy.
		VkBufferCopy copyRegion = VkBufferCopy();
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = vertexBufferSize;
		// Copy the data from the staging buffer to the vertex buffer
		m_deviceFunctions->vkCmdCopyBuffer(m_commandBuffer, stagingBuffer, m_vertexBuffer, 1, &copyRegion);

		// And again for the index buffer, offset by the size of the vertex buffer
		// Reusing the same copyRegion struct since vkCmdCopyBuffer is already done with it
		// Since we're copying to a different destination buffer, we need to keep the destination offset at 0 and set the source offset to the size of the vertex buffer
		copyRegion.srcOffset = vertexBufferSize;
		copyRegion.dstOffset = 0;
		copyRegion.size = indexBufferSize;
		m_deviceFunctions->vkCmdCopyBuffer(m_commandBuffer, stagingBuffer, m_indexBuffer, 1, &copyRegion);

		m_deviceFunctions->vkEndCommandBuffer(m_commandBuffer);

		// Configure the submit info for the command buffer
		VkCommandBufferSubmitInfo copyCommandInfo = VkCommandBufferSubmitInfo();
		copyCommandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		copyCommandInfo.commandBuffer = m_commandBuffer;

		// Copy the data from the staging buffer to the vertex buffer
		// No synchronization is used for this operation, since it is a one-time transfer and we don't need to synchronize with any other operations
		VkSubmitInfo2 copySubmitInfo = VkSubmitInfo2();
		copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		copySubmitInfo.commandBufferInfoCount = 1;
		copySubmitInfo.pCommandBufferInfos = &copyCommandInfo;

		VkResult copyResult = m_deviceFunctions->vkQueueSubmit2(m_queue, 1, &copySubmitInfo, VK_NULL_HANDLE);
		if(copyResult == VK_SUCCESS)
			m_deviceFunctions->vkQueueWaitIdle(m_queue); // Wait for the transfer to complete before cleaning up the staging buffer

		m_deviceFunctions->vkFreeMemory(m_device, stagingMemory, nullptr);
		m_deviceFunctions->vkDestroyBuffer(m_device, stagingBuffer, nullptr);

		if(copyResult != VK_SUCCESS)
		{
			return false; // TODO: Log this once error handling system exists.
		}

		m_indexCount = static_cast<std::uint32_t>(mesh.indexCount);

		return true;
	}

	void WindowSystem::onSetUp()
	{
		//////////////////
		// Initialize GLFW
		//////////////////

		// Initialize the GLFW context and validate that it was successful.
		if(glfwInit() == GLFW_FALSE)
			return; // TODO: Log this once error handling system exists.

		// Set configuration hints for the GLFW window. We don't need an OpenGL context since we'll be using Vulkan.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context; we'll use Vulkan later.
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: This is temporary, but the resizing mechanism comes later.

		// Create the window with the specified width, height, and title. The last two parameters are for monitor and shared context, which we don't need.
		// Monitor is for fullscreen mode, and share is a shared context is for sharing OpenGL contexts.
		m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, m_windowTitle.c_str(), nullptr, nullptr);

		if(!setUpVulkan())
		{
			return; // TODO: Log this once error handling system exists.
		}

		if(!setUpShaders())
		{
			return; // TODO: Log this once error handling system exists.
		}

		if(!setUpMeshBuffers())
		{
			return; // TODO: Log this once error handling system exists.
		}

		m_drawable.position = math::Vec3f(0.0f, 3.0f, 0.0f);
	}

	void WindowSystem::update(std::chrono::steady_clock::duration dt)
	{
		if(m_window == nullptr)
			return;

		glfwPollEvents();

		if(glfwWindowShouldClose(m_window) || glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			m_windowSystemBroadcaster.broadcast(*m_shutdownChannel, true);

		float deltaTime = std::chrono::duration<float>(dt).count();

		// Move the drawable based on keyboard input
		// Rotation up/down
		if(glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
			m_drawable.rotation.y += m_moveSpeed * deltaTime;
		else if(glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
			m_drawable.rotation.y -= m_moveSpeed * deltaTime;
		// Rotation left/right
		if(glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
			m_drawable.rotation.x -= m_moveSpeed * deltaTime;
		else if(glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
			m_drawable.rotation.x += m_moveSpeed * deltaTime;
		// Rotation roll
		if(glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS)
			m_drawable.rotation.z -= m_moveSpeed * deltaTime;
		else if(glfwGetKey(m_window, GLFW_KEY_E) == GLFW_PRESS)
			m_drawable.rotation.z += m_moveSpeed * deltaTime;

		// Translation up/down
		if(glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS)
			m_drawable.position.z += m_moveSpeed * deltaTime;
		else if(glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS)
			m_drawable.position.z -= m_moveSpeed * deltaTime;
		// Translation left/right
		if(glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS)
			m_drawable.position.x += m_moveSpeed * deltaTime;
		else if(glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS)
			m_drawable.position.x -= m_moveSpeed * deltaTime;
		// Translation forward/backward
		if(glfwGetKey(m_window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
			m_drawable.position.y += m_moveSpeed * deltaTime;
		else if(glfwGetKey(m_window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
			m_drawable.position.y -= m_moveSpeed * deltaTime;

		// Reset the drawable's position and rotation
		if(glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS)
		{
			m_drawable.position = math::Vec3f(0.0f, 3.0f, 0.0f);
			m_drawable.rotation = math::Vec3f(0.0f, 0.0f, 0.0f);
		}

		// Auto-rotate
		if(glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS)
		{
			m_drawable.rotation.z += m_spinSpeed * deltaTime;
			if(m_drawable.rotation.z > math::PI)
				m_drawable.rotation.z -= math::TAU;
			else if(m_drawable.rotation.z < -math::PI)
				m_drawable.rotation.z += math::TAU;
		}
	}

	void WindowSystem::render()
	{
		// Make sure setup was successful before trying to render. If not, just return.
		// The fence was the last thing created in Vulkan setup, so if it's nullptr, we know setup did not complete successfully.
		// TODO: We shouldn't need this. The engine should not call render() if setup failed. This is a temporary measure until the engine has a proper error handling system.
		if(m_frameFence == nullptr || m_pipeline == nullptr || m_vertexBuffer == nullptr || m_indexBuffer == nullptr)
			return;

		// Wait until the GPU has finished the previous frame.
		m_deviceFunctions->vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX);

		// Get the next image in the swapchain.
		std::uint32_t imageIndex = 0;
		// Wait on the semaphore that signals when the image is available.
		VkResult acquireResult = m_deviceFunctions->vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable, nullptr, &imageIndex);
		if(acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
		{
			// VK_SUBOPTIMAL_KHR is allowed, but it means the swapchain is no longer optimal for the surface. This can happen if the window is resized, for example.
			// There are a dozen codes that can be returned. See https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkAcquireNextImageKHR.html for the full list.
			return; // Skip this frame. TODO: Recreate the swapchain once resizing is implemented.
		}

		// This has to be done here, after the frame may be skipped, because if it were set first and the frame were skipped,
		// nothing would reset the fence and the next frame would wait forever.
		m_deviceFunctions->vkResetFences(m_device, 1, &m_frameFence);

		// Start recording commands
		VkCommandBufferBeginInfo beginInfo = VkCommandBufferBeginInfo();
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		// I don't understand why this is recreated every frame.
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		m_deviceFunctions->vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
		// Beginning clears out the command buffer because RESET_COMMAND_BUFFER_BIT was set when the command pool was created.
		// If it weren't set, then individual command buffers couldn't be reset and would have to be re-recorded every frame.

		// Prepare the image for rendering.
		VkImageMemoryBarrier2 toDrawing = VkImageMemoryBarrier2();
		toDrawing.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		// GPU operations run in stages.
		// See https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineStageFlagBits2.html for a list of stage bits.
		// The color attachment output stage is where the final color values are written to the image.
		toDrawing.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		toDrawing.srcAccessMask = 0;
		toDrawing.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		toDrawing.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		toDrawing.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // This value means we don't care about the previous data.
		toDrawing.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		toDrawing.image = m_swapchainImages[imageIndex];
		toDrawing.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		toDrawing.subresourceRange.levelCount = 1;
		toDrawing.subresourceRange.layerCount = 1;

		// No idea what this is. This is way too confusing
		VkDependencyInfo toDrawingDependency = VkDependencyInfo();
		toDrawingDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		toDrawingDependency.imageMemoryBarrierCount = 1;
		toDrawingDependency.pImageMemoryBarriers = &toDrawing;
		// Pass the magical dependency to the command buffer
		m_deviceFunctions->vkCmdPipelineBarrier2(m_commandBuffer, &toDrawingDependency);

		// Clear the image to a solid color.
		VkRenderingAttachmentInfo colorAttachment =  VkRenderingAttachmentInfo();
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		// Bind the current swapchain image to the color attachment.
		colorAttachment.imageView = m_swapchainImageViews[imageIndex];
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// CLEAR fills the image with  clearValue when drawing begins, and STORE keeps the result afterward.
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// RGBA
		colorAttachment.clearValue.color.float32[0] = 0.1f;
		colorAttachment.clearValue.color.float32[1] = 0.1f;
		colorAttachment.clearValue.color.float32[2] = 0.2f;
		colorAttachment.clearValue.color.float32[3] = 1.0f;

		VkRenderingInfo renderingInfo = VkRenderingInfo();
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.extent.width = m_swapchainWidth;
		renderingInfo.renderArea.extent.height = m_swapchainHeight;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		///////////////////////////
		// Shader pipeline phase //
		///////////////////////////
		m_deviceFunctions->vkCmdBeginRendering(m_commandBuffer, &renderingInfo);

		// Bind the shader pipeline from setUpShaders() to the command buffer
		// This tells Vulkan to use the specified pipeline for subsequent draw calls
		m_deviceFunctions->vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		VkViewport viewport = VkViewport();
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(m_swapchainHeight);
		viewport.width = static_cast<float>(m_swapchainWidth);
		viewport.height = -static_cast<float>(m_swapchainHeight); // Force Vulkan to treat the viewport as having the origin at bottom-left instead of top-left. This is necessary because Vulkan's coordinate system has the origin at the top-left, while OpenGL's coordinate system has the origin at the bottom-left. This is a common source of confusion when porting code between the two APIs.
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		// We only have one viewport, so we bind it to the command buffer at index 0. If we had multiple viewports, we would bind them at different indices.
		m_deviceFunctions->vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = VkRect2D();
		scissor.extent.width = m_swapchainWidth;
		scissor.extent.height = m_swapchainHeight;
		// Bind the single scissor rectangle to the command buffer
		m_deviceFunctions->vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

		VkDeviceSize vertexOffset = 0;
		// All vertex data is stored in a single buffer, so we bind it to the command buffer at binding index 0. If we had multiple vertex buffers, we would bind them at different indices.
		m_deviceFunctions->vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vertexBuffer, &vertexOffset);
		// Bind the index buffer to the command buffer. Only one index buffer can be bound at a time.
		// Index type is set to 32-bit unsigned integers, which matches the type of the indices in the mesh. UINT16 can also be used for smaller meshes, capping the maximum index value at 65535.
		m_deviceFunctions->vkCmdBindIndexBuffer(m_commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Identity matrix, written in column-major order.
		// CRITICALLY, that means each *row* of the initialization list is a *column* of the matrix.
		float oldmvp[4][4] = {
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}
		};
		// [column z][row z] = -0.5f;
		// Scale and flip the z-axis to fit the depth range of 0.0 to 1.0
		oldmvp[2][2] = -0.5f;
		// [column w][row z] = 0.5f;
		// Translate the z-axis to fit the depth range of 0.0 to 1.0
		oldmvp[3][2] = 0.5f; 

		// Fix aspect ratio of the viewport. Scale x by the ratio of height to width, so that the viewport is not stretched or squashed
		// The viewport is in normalized device coordinates, from -1 to 1 in both x and y, so it has no knowledge of the pixel dimensions of the viewport.
		oldmvp[0][0] = static_cast<float>(m_swapchainHeight) / static_cast<float>(m_swapchainWidth);

		math::Mat4f model = math::Mat4f::translation(m_drawable.position) * math::Mat4f::rotation(m_drawable.rotation);

		// Converting world space (z-up, y-forward) to camera space is a rotation of -90 degrees around the x-axis.
		math::Mat4f view = math::Mat4f:: rotation(-math::PI / 2.0f, 0.0f, 0.0f);

		// Calculate the aspect ratio of the viewport, height / width
		float aspectRatio = static_cast<float>(m_swapchainWidth) / static_cast<float>(m_swapchainHeight);
		// Perspective projection matrix, with a 60 degree field of view, and near and far planes at 0.1 and 100.0 units away from the camera.
		math::Mat4f projection = math::Mat4f::perspective(math::PI / 3.0f, aspectRatio, 0.1f, 100.0f);

		// Matrix transforms are applied right-to-left. This means that the model matrix is applied first, then the view matrix, then the projection matrix.
		math::Mat4f mvp = projection * view * model;

		// Copy the MVP matrix to the push constant range of the pipeline layout as a flat array of floats, as declared in the shader layout, in the vertex stage at offset 0.
		m_deviceFunctions->vkCmdPushConstants(m_commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp.mat), mvp.mat);

		// Draw the mesh using the index buffer
		m_deviceFunctions->vkCmdDrawIndexed(m_commandBuffer, m_indexCount, 1, 0, 0, 0);
		// End the rendering phase. This finalizes the rendering commands and prepares the image for presentation.
		m_deviceFunctions->vkCmdEndRendering(m_commandBuffer);

		////////////////////////
		// Presentation Phase //
		////////////////////////
		// Prepare the image for presentation.
		// A barrier synchronizes phases of the pipeline, dependent on the stage and access masks set. It ensures that required writes are completed before the reads begin, and that the data is visible to the next stage of the pipeline.
		// In terms of stages and masks:
		// After the `srcStage` step finishes,
		// flush its `srcAccess` writes;
		// then let the `dstStage` step begin,
		// with fresh data for its `dstAccess` reads or writes.
		
		// For this barrier:
		// After the color output step finishes,
		// flush its color writes;
		// then let nothing in particular begin, with nothing to refresh.
		VkImageMemoryBarrier2 toPresent = VkImageMemoryBarrier2();
		toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		// 
		toPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		// ???
		toPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		toPresent.dstAccessMask = 0;
		toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		toPresent.image = m_swapchainImages[imageIndex];
		// Where do the subresource values come from?
		toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		toPresent.subresourceRange.levelCount = 1;
		toPresent.subresourceRange.layerCount = 1;

		VkDependencyInfo toPresentDependency = VkDependencyInfo();
		toPresentDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		toPresentDependency.imageMemoryBarrierCount = 1;
		toPresentDependency.pImageMemoryBarriers = &toPresent;
		m_deviceFunctions->vkCmdPipelineBarrier2(m_commandBuffer, &toPresentDependency);

		m_deviceFunctions->vkEndCommandBuffer(m_commandBuffer);

		// Info structs for submitting the semaphores and command buffer to the queue.
		// Configure a semaphore to wait on the color attachment output stage
		VkSemaphoreSubmitInfo waitInfo = VkSemaphoreSubmitInfo();
		waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitInfo.semaphore = m_imageAvailable;
		waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		// Configure a semaphore to signal when the rendering is finished (all commands have been executed and the image is ready for presentation)
		// When passed as a signal semaphore to vkQueueSubmit2, the queue will signal the semaphore when the command buffer has finished executing.
		VkSemaphoreSubmitInfo signalInfo = VkSemaphoreSubmitInfo();
		signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalInfo.semaphore = m_renderFinished[imageIndex];
		signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkCommandBufferSubmitInfo commandInfo = VkCommandBufferSubmitInfo();
		commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		commandInfo.commandBuffer = m_commandBuffer;

		// Bind the info structs to a submit info struct
		VkSubmitInfo2 submitInfo = VkSubmitInfo2();
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submitInfo.waitSemaphoreInfoCount = 1;
		submitInfo.pWaitSemaphoreInfos = &waitInfo;
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &commandInfo;
		submitInfo.signalSemaphoreInfoCount = 1;
		submitInfo.pSignalSemaphoreInfos = &signalInfo;

		// Submit the work to the queue and signal the fence when it is done.
		m_deviceFunctions->vkQueueSubmit2(m_queue, 1, &submitInfo, m_frameFence);

		// Present the image to the surface.
		VkPresentInfoKHR presentInfo = VkPresentInfoKHR();
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// Wait on the semaphore that signals when rendering is finished.
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinished[imageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;

		// Present the image to the surface.
		m_deviceFunctions->vkQueuePresentKHR(m_queue, &presentInfo);
	}

	void WindowSystem::tearDownMeshBuffers()
	{
		if(m_device == nullptr)
			return;

		if(m_indexMemory != nullptr)
		{
			m_deviceFunctions->vkFreeMemory(m_device, m_indexMemory, nullptr);
			m_indexMemory = nullptr;
		}
		if(m_indexBuffer != nullptr)
		{
			m_deviceFunctions->vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
			m_indexBuffer = nullptr;
		}
		if(m_vertexMemory != nullptr)
		{
			m_deviceFunctions->vkFreeMemory(m_device, m_vertexMemory, nullptr);
			m_vertexMemory = nullptr;
		}
		if(m_vertexBuffer != nullptr)
		{
			m_deviceFunctions->vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
			m_vertexBuffer = nullptr;
		}
	}

	void WindowSystem::tearDownShaders()
	{
		if(m_device	== nullptr)
			return;
		
		if(m_pipeline != nullptr)
		{
			m_deviceFunctions->vkDestroyPipeline(m_device, m_pipeline, nullptr);
			m_pipeline = nullptr;
		}
		if(m_pipelineLayout != nullptr)
		{
			m_deviceFunctions->vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
			m_pipelineLayout = nullptr;
		}
	}

	void WindowSystem::tearDownVulkan()
	{
		if(m_frameFence != nullptr)
		{
			m_deviceFunctions->vkDestroyFence(m_device, m_frameFence, nullptr);
			m_frameFence = nullptr;
		}

		if(m_renderFinished != nullptr)
		{
			for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
			{
				if(m_renderFinished[imageIdx] != nullptr)
					m_deviceFunctions->vkDestroySemaphore(m_device, m_renderFinished[imageIdx], nullptr);
			}
			delete[] m_renderFinished;
			m_renderFinished = nullptr;
		}

		if(m_imageAvailable != nullptr)
		{
			m_deviceFunctions->vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
			m_imageAvailable = nullptr;
		}

		if(m_commandPool != nullptr)
		{
			m_deviceFunctions->vkDestroyCommandPool(m_device, m_commandPool, nullptr);
			m_commandPool = nullptr;
			m_commandBuffer = nullptr; // Command buffers are freed when the pool is destroyed, so we don't need to free it explicitly.
		}

		// Clean up Vulkan resources and destroy the Vulkan instance.
		if(m_swapchainImageViews != nullptr)
		{
			for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
			{
				if(m_swapchainImageViews[imageIdx] != nullptr)
					m_deviceFunctions->vkDestroyImageView(m_device, m_swapchainImageViews[imageIdx], nullptr);
			}
			delete[] m_swapchainImageViews;
			m_swapchainImageViews = nullptr;
		}

		if(m_swapchainImages != nullptr)
		{
			delete[] m_swapchainImages;
			m_swapchainImages = nullptr;
		}

		if(m_swapchain != nullptr)
		{
			m_deviceFunctions->vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
			m_swapchain = nullptr;
		}

		if(m_device != nullptr)
		{
			m_deviceFunctions->vkDestroyDevice(m_device, nullptr);
			m_device = nullptr;
		}

		if(m_surface != nullptr)
		{
			m_vulkan->vkDestroySurfaceKHR(m_vkInstance, m_surface, nullptr);
			m_surface = nullptr;
		}
		if(m_vkInstance != nullptr)
		{
			m_vulkan->vkDestroyInstance(m_vkInstance, nullptr);
			m_vkInstance = nullptr;
		}

		delete m_deviceFunctions;
		m_deviceFunctions = nullptr;

		delete m_vulkan;
		m_vulkan = nullptr;
	}

	void WindowSystem::onTearDown()
	{
		// Synchronize before destroying resources to ensure that all GPU operations are complete.
		if(m_device != nullptr)
		{
			m_deviceFunctions->vkDeviceWaitIdle(m_device);
		}
		tearDownMeshBuffers();
		tearDownShaders();
		tearDownVulkan();
		// Clean up GLFW resources and terminate the GLFW context.
		if(m_window != nullptr)
		{
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
		glfwTerminate();
	}
}